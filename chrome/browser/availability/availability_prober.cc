// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/availability/availability_prober.h"

#include <math.h>

#include <cmath>

#include "base/base64.h"
#include "base/bind.h"
#include "base/guid.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/availability/proto/availability_prober_cache_entry.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#include "net/base/network_interfaces.h"
#endif

namespace {

const char kCachePrefKeyPrefix[] = "Availability.Prober.cache";

const char kSuccessHistogram[] = "Availability.Prober.DidSucceed";
const char kSuccessAfterReportedFailure[] =
    "Availability.Prober.DidSucceed.AfterReportedFailure";
const char kFinalResultHistogram[] = "Availability.Prober.FinalState";
const char kTimeUntilSuccess[] = "Availability.Prober.TimeUntilSuccess2";
const char kTimeUntilFailure[] = "Availability.Prober.TimeUntilFailure2";
const char kAttemptsBeforeSuccessHistogram[] =
    "Availability.Prober.NumAttemptsBeforeSuccess";
const char kHttpRespCodeHistogram[] = "Availability.Prober.ResponseCode";
const char kNetErrorHistogram[] = "Availability.Prober.NetError";
const char kCacheEntryAgeHistogram[] = "Availability.Prober.CacheEntryAge";

// Please keep this up to date with logged histogram suffix
// |Availability.Prober.Clients| in tools/metrics/histograms/histograms.xml.
// These names are also used in prefs so they should not be changed without
// consideration for removing the old value.
std::string NameForClient(AvailabilityProber::ClientName name) {
  switch (name) {
    case AvailabilityProber::ClientName::kLitepages:
      return "Litepages";
    case AvailabilityProber::ClientName::kLitepagesOriginCheck:
      return "LitepagesOriginCheck";
  }
  NOTREACHED();
  return std::string();
}

std::string PrefKeyForName(const std::string& name) {
  return base::StringPrintf("%s.%s", kCachePrefKeyPrefix, name.c_str());
}

std::string HttpMethodToString(AvailabilityProber::HttpMethod http_method) {
  switch (http_method) {
    case AvailabilityProber::HttpMethod::kGet:
      return "GET";
    case AvailabilityProber::HttpMethod::kHead:
      return "HEAD";
  }
}

// Computes the time delta for a given Backoff algorithm, a base interval, and
// the count of how many attempts have been made thus far.
base::TimeDelta ComputeNextTimeDeltaForBackoff(
    AvailabilityProber::Backoff backoff,
    base::TimeDelta base_interval,
    size_t attempts_so_far) {
  switch (backoff) {
    case AvailabilityProber::Backoff::kLinear:
      return base_interval;
    case AvailabilityProber::Backoff::kExponential:
      return base_interval * pow(2, attempts_so_far);
  }
}

std::string GenerateNetworkID(
    network::NetworkConnectionTracker* network_connection_tracker) {
  network::mojom::ConnectionType connection_type =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  if (network_connection_tracker) {
    network_connection_tracker->GetConnectionType(&connection_type,
                                                  base::DoNothing());
  }

  std::string id = base::NumberToString(static_cast<int>(connection_type));
  bool is_cellular =
      network::NetworkConnectionTracker::IsConnectionCellular(connection_type);
  if (is_cellular) {
    // Don't care about cell connection type.
    id = "cell";
  }

// Further identify WiFi and cell connections. These calls are only supported
// for Android devices.
#if defined(OS_ANDROID)
  if (connection_type == network::mojom::ConnectionType::CONNECTION_WIFI) {
    return base::StringPrintf("%s,%s", id.c_str(), net::GetWifiSSID().c_str());
  }

  if (is_cellular) {
    return base::StringPrintf(
        "%s,%s", id.c_str(),
        net::android::GetTelephonyNetworkOperator().c_str());
  }
#endif

  return id;
}

base::Optional<base::Value> EncodeCacheEntryValue(
    const AvailabilityProberCacheEntry& entry) {
  std::string serialized_entry;
  bool serialize_to_string_ok = entry.SerializeToString(&serialized_entry);
  if (!serialize_to_string_ok)
    return base::nullopt;

  std::string base64_encoded;
  base::Base64Encode(serialized_entry, &base64_encoded);
  return base::Value(base64_encoded);
}

base::Optional<AvailabilityProberCacheEntry> DecodeCacheEntryValue(
    const base::Value& value) {
  if (!value.is_string())
    return base::nullopt;

  std::string base64_decoded;
  if (!base::Base64Decode(value.GetString(), &base64_decoded))
    return base::nullopt;

  AvailabilityProberCacheEntry entry;
  if (!entry.ParseFromString(base64_decoded))
    return base::nullopt;

  return entry;
}

base::Time LastModifiedTimeFromCacheEntry(
    const AvailabilityProberCacheEntry& entry) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(entry.last_modified()));
}

void RemoveOldestDictionaryEntry(base::DictionaryValue* dict) {
  std::vector<std::string> keys_to_remove;

  std::string oldest_key;
  base::Time oldest_mod_time = base::Time::Max();
  for (const auto& iter : dict->DictItems()) {
    base::Optional<AvailabilityProberCacheEntry> entry =
        DecodeCacheEntryValue(iter.second);
    if (!entry.has_value()) {
      // Also remove anything that can't be decoded.
      keys_to_remove.push_back(iter.first);
      continue;
    }

    base::Time mod_time = LastModifiedTimeFromCacheEntry(entry.value());
    if (mod_time < oldest_mod_time) {
      oldest_key = iter.first;
      oldest_mod_time = mod_time;
    }
  }

  if (!oldest_key.empty()) {
    keys_to_remove.push_back(oldest_key);
  }

  for (const std::string& key : keys_to_remove) {
    dict->RemoveKey(key);
  }
}

#if defined(OS_ANDROID)
bool IsInForeground(base::android::ApplicationState state) {
  switch (state) {
    case base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES:
      return true;
    case base::android::APPLICATION_STATE_UNKNOWN:
    case base::android::APPLICATION_STATE_HAS_PAUSED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES:
    case base::android::APPLICATION_STATE_HAS_DESTROYED_ACTIVITIES:
      return false;
  }
}
#endif

}  // namespace

AvailabilityProber::RetryPolicy::RetryPolicy() = default;
AvailabilityProber::RetryPolicy::~RetryPolicy() = default;
AvailabilityProber::RetryPolicy::RetryPolicy(
    AvailabilityProber::RetryPolicy const&) = default;
AvailabilityProber::TimeoutPolicy::TimeoutPolicy() = default;
AvailabilityProber::TimeoutPolicy::~TimeoutPolicy() = default;
AvailabilityProber::TimeoutPolicy::TimeoutPolicy(
    AvailabilityProber::TimeoutPolicy const&) = default;

AvailabilityProber::AvailabilityProber(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    const ClientName name,
    const GURL& url,
    const HttpMethod http_method,
    const net::HttpRequestHeaders headers,
    const RetryPolicy& retry_policy,
    const TimeoutPolicy& timeout_policy,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const size_t max_cache_entries,
    base::TimeDelta revalidate_cache_after)
    : AvailabilityProber(delegate,
                         url_loader_factory,
                         pref_service,
                         name,
                         url,
                         http_method,
                         headers,
                         retry_policy,
                         timeout_policy,
                         traffic_annotation,
                         max_cache_entries,
                         revalidate_cache_after,
                         base::DefaultTickClock::GetInstance(),
                         base::DefaultClock::GetInstance()) {}

AvailabilityProber::AvailabilityProber(
    Delegate* delegate,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    const ClientName name,
    const GURL& url,
    const HttpMethod http_method,
    const net::HttpRequestHeaders headers,
    const RetryPolicy& retry_policy,
    const TimeoutPolicy& timeout_policy,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    const size_t max_cache_entries,
    base::TimeDelta revalidate_cache_after,
    const base::TickClock* tick_clock,
    const base::Clock* clock)
    : delegate_(delegate),
      name_(NameForClient(name)),
      pref_key_(PrefKeyForName(NameForClient(name))),
      url_(url),
      http_method_(http_method),
      headers_(headers),
      retry_policy_(retry_policy),
      timeout_policy_(timeout_policy),
      max_cache_entries_(max_cache_entries),
      revalidate_cache_after_(revalidate_cache_after),
      traffic_annotation_(traffic_annotation),
      successive_retry_count_(0),
      successive_timeout_count_(0),
      cached_probe_results_(std::make_unique<base::DictionaryValue>()),
      tick_clock_(tick_clock),
      clock_(clock),
      network_connection_tracker_(nullptr),
      pref_service_(pref_service),
      url_loader_factory_(url_loader_factory),
      reported_external_failure_(false),
      weak_factory_(this) {
  DCHECK(delegate_);

  // The NetworkConnectionTracker can only be used directly on the UI thread.
  // Otherwise we use the cross-thread call.
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) &&
      content::GetNetworkConnectionTracker()) {
    AddSelfAsNetworkConnectionObserver(content::GetNetworkConnectionTracker());
  } else {
    content::GetNetworkConnectionTrackerFromUIThread(
        base::BindOnce(&AvailabilityProber::AddSelfAsNetworkConnectionObserver,
                       weak_factory_.GetWeakPtr()));
  }

  if (pref_service_) {
    cached_probe_results_ =
        pref_service_->GetDictionary(pref_key_)->CreateDeepCopy();
  }
}

AvailabilityProber::~AvailabilityProber() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_connection_tracker_)
    network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

// static
void AvailabilityProber::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  for (int i = 0;
       i <= static_cast<int>(AvailabilityProber::ClientName::kMaxValue); i++) {
    registry->RegisterDictionaryPref(PrefKeyForName(
        NameForClient(static_cast<AvailabilityProber::ClientName>(i))));
  }
}

// static
void AvailabilityProber::ClearData(PrefService* pref_service) {
  for (int i = 0;
       i <= static_cast<int>(AvailabilityProber::ClientName::kMaxValue); i++) {
    std::string key = PrefKeyForName(
        NameForClient(static_cast<AvailabilityProber::ClientName>(i)));
    DictionaryPrefUpdate update(pref_service, key);
    update.Get()->Clear();
  }
}

void AvailabilityProber::AddSelfAsNetworkConnectionObserver(
    network::NetworkConnectionTracker* network_connection_tracker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  network_connection_tracker_ = network_connection_tracker;
  network_connection_tracker_->AddNetworkConnectionObserver(this);
}

void AvailabilityProber::OnProbingEnd() {
  base::Value* cache_entry =
      cached_probe_results_->FindKey(GetCacheKeyForCurrentNetwork());
  if (cache_entry) {
    base::Optional<AvailabilityProberCacheEntry> entry =
        DecodeCacheEntryValue(*cache_entry);
    if (entry.has_value()) {
      base::BooleanHistogram::FactoryGet(
          AppendNameToHistogram(kFinalResultHistogram),
          base::HistogramBase::kUmaTargetedHistogramFlag)
          ->Add(entry.value().is_success());
    }
  }

  ResetState();
}

void AvailabilityProber::ResetState() {
  time_when_set_active_ = base::nullopt;
  successive_retry_count_ = 0;
  successive_timeout_count_ = 0;
  retry_timer_.reset();
  timeout_timer_.reset();
  url_loader_.reset();
  reported_external_failure_ = false;
#if defined(OS_ANDROID)
  application_status_listener_.reset();
#endif
}

void AvailabilityProber::SendNowIfInactive(bool send_only_in_foreground) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (time_when_set_active_.has_value())
    return;

#if defined(OS_ANDROID)
  if (send_only_in_foreground &&
      !IsInForeground(base::android::ApplicationStatusListener::GetState())) {
    // base::Unretained is safe here because the callback is owned by
    // |application_status_listener_| which is owned by |this|.
    application_status_listener_ =
        base::android::ApplicationStatusListener::New(
            base::BindRepeating(&AvailabilityProber::OnApplicationStateChange,
                                base::Unretained(this)));
    return;
  }
#endif

  CreateAndStartURLLoader();
}

void AvailabilityProber::RepeatedlyProbe(base::TimeDelta interval,
                                         bool send_only_in_foreground) {
  repeating_timer_ = std::make_unique<base::RepeatingTimer>(tick_clock_);
  // base::Unretained is safe here because |repeating_timer_| is owned by
  // |this|.
  repeating_timer_->Start(
      FROM_HERE, interval,
      base::BindRepeating(&AvailabilityProber::SendNowIfInactive,
                          base::Unretained(this), send_only_in_foreground));

  SendNowIfInactive(send_only_in_foreground);
}

#if defined(OS_ANDROID)
void AvailabilityProber::OnApplicationStateChange(
    base::android::ApplicationState new_state) {
  DCHECK(application_status_listener_);

  if (!IsInForeground(new_state))
    return;

  SendNowIfInactive(false);
  application_status_listener_.reset();
}
#endif

void AvailabilityProber::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // If a probe is already in flight we don't want to continue to use it since
  // the network has just changed. Reset all state and start again.
  ResetState();
  CreateAndStartURLLoader();
}

void AvailabilityProber::CreateAndStartURLLoader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!time_when_set_active_.has_value() || successive_retry_count_ > 0);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!url_loader_);

  if (!delegate_->ShouldSendNextProbe()) {
    OnProbingEnd();
    return;
  }

  time_when_set_active_ = clock_->Now();

  GURL url = url_;
  if (retry_policy_.use_random_urls) {
    std::string query = "guid=" + base::GenerateGUID();
    GURL::Replacements replacements;
    replacements.SetQuery(query.c_str(), url::Component(0, query.length()));
    url = url.ReplaceComponents(replacements);
  }

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = HttpMethodToString(http_method_);
  request->headers = headers_;
  request->load_flags = net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);

  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&AvailabilityProber::OnURLLoadComplete,
                     base::Unretained(this)),
      1024);

  // We don't use SimpleURLLoader's timeout functionality because it is not
  // possible to test by AvailabilityProberTest.
  base::TimeDelta ttl = ComputeNextTimeDeltaForBackoff(
      timeout_policy_.backoff, timeout_policy_.base_timeout,
      successive_timeout_count_);
  timeout_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
  // base::Unretained is safe because |timeout_timer_| is owned by this.
  timeout_timer_->Start(FROM_HERE, ttl,
                        base::BindOnce(&AvailabilityProber::ProcessProbeTimeout,
                                       base::Unretained(this)));
}

void AvailabilityProber::OnURLLoadComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  int response_code = -1;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();

    base::UmaHistogramSparse(AppendNameToHistogram(kHttpRespCodeHistogram),
                             std::abs(response_code));
  }

  base::UmaHistogramSparse(AppendNameToHistogram(kNetErrorHistogram),
                           std::abs(url_loader_->NetError()));

  bool was_successful = delegate_->IsResponseSuccess(
      static_cast<net::Error>(url_loader_->NetError()),
      url_loader_->ResponseInfo(), std::move(response_body));

  timeout_timer_.reset();
  url_loader_.reset();
  successive_timeout_count_ = 0;

  if (was_successful) {
    ProcessProbeSuccess();
    return;
  }
  ProcessProbeFailure();
}

void AvailabilityProber::ProcessProbeTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(url_loader_);

  // Since we manually set the timeout handling of the probe, record the net
  // error here as well for simplicity.
  base::UmaHistogramSparse(AppendNameToHistogram(kNetErrorHistogram),
                           std::abs(net::ERR_TIMED_OUT));

  url_loader_.reset();
  successive_timeout_count_++;
  ProcessProbeFailure();
}

void AvailabilityProber::ProcessProbeFailure() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(!url_loader_);
  DCHECK(time_when_set_active_.has_value());

  RecordProbeResult(false);

  DCHECK(time_when_set_active_.has_value());
  if (time_when_set_active_.has_value()) {
    base::TimeDelta active_time = clock_->Now() - time_when_set_active_.value();
    base::Histogram::FactoryTimeGet(
        AppendNameToHistogram(kTimeUntilFailure),
        base::TimeDelta::FromMilliseconds(0) /* minimum */,
        base::TimeDelta::FromMilliseconds(60000) /* maximum */,
        50 /* bucket_count */, base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(active_time.InMilliseconds());
  }

  if (retry_policy_.max_retries > successive_retry_count_) {
    base::TimeDelta interval = ComputeNextTimeDeltaForBackoff(
        retry_policy_.backoff, retry_policy_.base_interval,
        successive_retry_count_);

    retry_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
    // base::Unretained is safe because |retry_timer_| is owned by this.
    retry_timer_->Start(
        FROM_HERE, interval,
        base::BindOnce(&AvailabilityProber::CreateAndStartURLLoader,
                       base::Unretained(this)));

    successive_retry_count_++;
    return;
  }

  OnProbingEnd();
}

void AvailabilityProber::ProcessProbeSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(!url_loader_);
  DCHECK(time_when_set_active_.has_value());

  base::LinearHistogram::FactoryGet(
      AppendNameToHistogram(kAttemptsBeforeSuccessHistogram), 1 /* minimum */,
      25 /* maximum */, 25 /* bucket_count */,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      // |successive_retry_count_| is zero when the first attempt is successful.
      // Increase by one for more intuitive metrics.
      ->Add(successive_retry_count_ + 1);

  DCHECK(time_when_set_active_.has_value());
  if (time_when_set_active_.has_value()) {
    base::TimeDelta active_time = clock_->Now() - time_when_set_active_.value();
    base::Histogram::FactoryTimeGet(
        AppendNameToHistogram(kTimeUntilSuccess),
        base::TimeDelta::FromMilliseconds(0) /* minimum */,
        base::TimeDelta::FromMilliseconds(30000) /* maximum */,
        50 /* bucket_count */, base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(active_time.InMilliseconds());
  }

  RecordProbeResult(true);
  OnProbingEnd();
}

base::Optional<bool> AvailabilityProber::LastProbeWasSuccessful() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value* cache_entry =
      cached_probe_results_->FindKey(GetCacheKeyForCurrentNetwork());
  if (!cache_entry)
    return base::nullopt;

  base::Optional<AvailabilityProberCacheEntry> entry =
      DecodeCacheEntryValue(*cache_entry);
  if (!entry.has_value())
    return base::nullopt;

  base::TimeDelta cache_entry_age =
      clock_->Now() - LastModifiedTimeFromCacheEntry(entry.value());

  base::LinearHistogram::FactoryTimeGet(
      AppendNameToHistogram(kCacheEntryAgeHistogram),
      base::TimeDelta::FromHours(0) /* minimum */,
      base::TimeDelta::FromHours(72) /* maximum */, 50 /* bucket_count */,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(cache_entry_age.InHours());

  // Check if the cache entry should be revalidated because it has expired or
  // cache_entry_age is negative because the clock was moved back.
  if (cache_entry_age >= revalidate_cache_after_ ||
      cache_entry_age < base::TimeDelta()) {
    SendNowIfInactive(false);
  }

  return entry.value().is_success();
}

void AvailabilityProber::ReportExternalFailureAndRetry() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  RecordProbeResult(false);
  reported_external_failure_ = true;
  SendNowIfInactive(false);
}

void AvailabilityProber::SetOnCompleteCallback(
    AvailabilityProberOnCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_complete_callback_ = std::move(callback);
}

void AvailabilityProber::RecordProbeResult(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AvailabilityProberCacheEntry entry;
  entry.set_is_success(success);
  entry.set_last_modified(
      clock_->Now().ToDeltaSinceWindowsEpoch().InMicroseconds());

  base::Optional<base::Value> encoded = EncodeCacheEntryValue(entry);
  if (!encoded.has_value()) {
    NOTREACHED();
    return;
  }

  base::DictionaryValue* update_dict = cached_probe_results_.get();
  if (pref_service_) {
    DictionaryPrefUpdate update(pref_service_, pref_key_);
    update_dict = update.Get();
  }

  update_dict->SetKey(GetCacheKeyForCurrentNetwork(),
                      std::move(encoded.value()));

  if (update_dict->DictSize() > max_cache_entries_)
    RemoveOldestDictionaryEntry(update_dict);

  cached_probe_results_ = update_dict->CreateDeepCopy();

  base::BooleanHistogram::FactoryGet(
      AppendNameToHistogram(kSuccessHistogram),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(success);

  if (reported_external_failure_) {
    base::BooleanHistogram::FactoryGet(
        AppendNameToHistogram(kSuccessAfterReportedFailure),
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(success);
  }

  // The callback may delete |this| so run it in a post task.
  if (on_complete_callback_) {
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(&AvailabilityProber::RunCallback,
                                  weak_factory_.GetWeakPtr(), success));
  }
}

void AvailabilityProber::RunCallback(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(on_complete_callback_).Run(success);
}

std::string AvailabilityProber::GetCacheKeyForCurrentNetwork() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::StringPrintf(
      "%s;%s:%d", GenerateNetworkID(network_connection_tracker_).c_str(),
      url_.host().c_str(), url_.EffectiveIntPort());
}

std::string AvailabilityProber::AppendNameToHistogram(
    const std::string& histogram) const {
  return base::StringPrintf("%s.%s", histogram.c_str(), name_.c_str());
}
