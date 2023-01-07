// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_canary_checker.h"

#include <math.h>

#include <cmath>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/guid.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_dns_prober.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/android/network_library.h"
#include "net/base/network_interfaces.h"
#endif

namespace {

// The maximum number of canary checks to cache. Each entry corresponds to
// a network the user was on during a single Chrome session, and cache misses
// are cheap so there's no reason to use a large value.
const size_t kMaxCacheSize = 4;

const char kFinalResultHistogram[] = "PrefetchProxy.CanaryChecker.FinalState";
const char kTimeUntilSuccess[] = "PrefetchProxy.CanaryChecker.TimeUntilSuccess";
const char kTimeUntilFailure[] = "PrefetchProxy.CanaryChecker.TimeUntilFailure";
const char kAttemptsBeforeSuccessHistogram[] =
    "PrefetchProxy.CanaryChecker.NumAttemptsBeforeSuccess";
const char kNetErrorHistogram[] = "PrefetchProxy.CanaryChecker.NetError";
const char kCacheEntryAgeHistogram[] =
    "PrefetchProxy.CanaryChecker.CacheEntryAge";
const char kCacheLookupResult[] =
    "PrefetchProxy.CanaryChecker.CacheLookupResult";

// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class CanaryCheckLookupResult {
  kSuccess = 0,
  kFailure = 1,
  kCacheMiss = 2,
  kMaxValue = kCacheMiss,
};

// Please keep this up to date with logged histogram suffix
// |PrefetchProxy.CanaryChecker.Clients| in
// //tools/metrics/histograms/metadata/prefetch/histograms.xml.
std::string NameForClient(PrefetchProxyCanaryChecker::CheckType name) {
  switch (name) {
    case PrefetchProxyCanaryChecker::CheckType::kTLS:
      return "TLS";
    case PrefetchProxyCanaryChecker::CheckType::kDNS:
      return "DNS";
    default:
      NOTREACHED() << static_cast<int>(name);
      return std::string();
  }
  NOTREACHED();
  return std::string();
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
#if BUILDFLAG(IS_ANDROID)
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

}  // namespace

PrefetchProxyCanaryChecker::RetryPolicy::RetryPolicy() = default;
PrefetchProxyCanaryChecker::RetryPolicy::~RetryPolicy() = default;
PrefetchProxyCanaryChecker::RetryPolicy::RetryPolicy(
    PrefetchProxyCanaryChecker::RetryPolicy const&) = default;

PrefetchProxyCanaryChecker::PrefetchProxyCanaryChecker(
    Profile* profile,
    CheckType name,
    const GURL& url,
    const RetryPolicy& retry_policy,
    base::TimeDelta check_timeout,
    base::TimeDelta revalidate_cache_after)
    : PrefetchProxyCanaryChecker(profile,
                                 name,
                                 url,
                                 retry_policy,
                                 check_timeout,
                                 revalidate_cache_after,
                                 base::DefaultTickClock::GetInstance(),
                                 base::DefaultClock::GetInstance()) {}

PrefetchProxyCanaryChecker::PrefetchProxyCanaryChecker(
    Profile* profile,
    CheckType name,
    const GURL& url,
    const RetryPolicy& retry_policy,
    base::TimeDelta check_timeout,
    base::TimeDelta revalidate_cache_after,
    const base::TickClock* tick_clock,
    const base::Clock* clock)
    : profile_(profile),
      name_(NameForClient(name)),
      url_(url),
      retry_policy_(retry_policy),
      backoff_entry_(&retry_policy_.backoff_policy),
      check_timeout_(check_timeout),
      revalidate_cache_after_(revalidate_cache_after),
      tick_clock_(tick_clock),
      clock_(clock),
      cache_(kMaxCacheSize),
      weak_factory_(this) {
  // The NetworkConnectionTracker can only be used directly on the UI thread.
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  network_connection_tracker_ = content::GetNetworkConnectionTracker();
  DCHECK(network_connection_tracker_);
}

PrefetchProxyCanaryChecker::~PrefetchProxyCanaryChecker() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::WeakPtr<PrefetchProxyCanaryChecker>
PrefetchProxyCanaryChecker::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void PrefetchProxyCanaryChecker::UpdateCacheEntry(
    PrefetchProxyCanaryChecker::CacheEntry entry,
    std::string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  latest_cache_key_ = key;
  cache_.Put(key, entry);
}

void PrefetchProxyCanaryChecker::UpdateCacheKey(std::string key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  latest_cache_key_ = key;
}

void PrefetchProxyCanaryChecker::OnCheckEnd(bool success) {
  PrefetchProxyCanaryChecker::CacheEntry entry;
  entry.success = success;
  entry.last_modified = clock_->Now();

  // We have the check result and we need to store it in the cache, keyed on
  // the current network key. Getting the network key on Android can be slow
  // so we do this asynchronously. Note that this is fundamentally racy: the
  // network might have changed since we completed the check. Fortunately, the
  // impact of using the wrong key is limited: we might simply filter probe when
  // we don't have to or fail to filter probe when we should.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(GenerateNetworkID, network_connection_tracker_),
      base::BindOnce(&PrefetchProxyCanaryChecker::UpdateCacheEntry,
                     weak_factory_.GetWeakPtr(), entry));

  DCHECK(time_when_set_active_.has_value());
  base::TimeDelta active_time = clock_->Now() - time_when_set_active_.value();
  if (success) {
    base::Histogram::FactoryTimeGet(
        AppendNameToHistogram(kTimeUntilSuccess),
        base::Milliseconds(0) /* minimum */,
        base::Milliseconds(30000) /* maximum */, 50 /* bucket_count */,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(active_time.InMilliseconds());
  } else {
    base::Histogram::FactoryTimeGet(
        AppendNameToHistogram(kTimeUntilFailure),
        base::Milliseconds(0) /* minimum */,
        base::Milliseconds(60000) /* maximum */, 50 /* bucket_count */,
        base::HistogramBase::kUmaTargetedHistogramFlag)
        ->Add(active_time.InMilliseconds());
  }
  base::BooleanHistogram::FactoryGet(
      AppendNameToHistogram(kFinalResultHistogram),
      base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(success);

  ResetState();
}

void PrefetchProxyCanaryChecker::ResetState() {
  time_when_set_active_ = absl::nullopt;
  resolver_control_handle_.reset();
  retry_timer_.reset();
  timeout_timer_.reset();
  backoff_entry_.Reset();
}

void PrefetchProxyCanaryChecker::SendNowIfInactive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (time_when_set_active_.has_value()) {
    // We already have an active check.
    return;
  }
  time_when_set_active_ = clock_->Now();

  StartDNSResolution(url_);
}

void PrefetchProxyCanaryChecker::ProcessTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Cancel the pending resolving job. This will do nothing if resolving has
  // already completed. Otherwise, the callback we registered (OnDNSResolved)
  // will be called with the error code we pass here (net::ERR_TIMED_OUT).
  resolver_control_handle_->Cancel(net::ERR_TIMED_OUT);
}

void PrefetchProxyCanaryChecker::ProcessFailure(int net_error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(time_when_set_active_.has_value());

  backoff_entry_.InformOfRequest(false);

  base::UmaHistogramSparse(AppendNameToHistogram(kNetErrorHistogram),
                           std::abs(net_error));

  if (retry_policy_.max_retries >=
      static_cast<size_t>(backoff_entry_.failure_count())) {
    base::TimeDelta interval = backoff_entry_.GetTimeUntilRelease();

    retry_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
    // base::Unretained is safe because |retry_timer_| is owned by this.
    retry_timer_->Start(
        FROM_HERE, interval,
        base::BindOnce(&PrefetchProxyCanaryChecker::StartDNSResolution,
                       base::Unretained(this), url_));
    return;
  }

  OnCheckEnd(false);
}

void PrefetchProxyCanaryChecker::ProcessSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!retry_timer_ || !retry_timer_->IsRunning());
  DCHECK(!timeout_timer_ || !timeout_timer_->IsRunning());
  DCHECK(time_when_set_active_.has_value());

  base::LinearHistogram::FactoryGet(
      AppendNameToHistogram(kAttemptsBeforeSuccessHistogram), 1 /* minimum */,
      25 /* maximum */, 25 /* bucket_count */,
      base::HistogramBase::kUmaTargetedHistogramFlag)
      // |failure_count| is zero when the first attempt is successful.
      // Increase by one for more intuitive metrics.
      ->Add(backoff_entry_.failure_count() + 1);

  OnCheckEnd(true);
}

absl::optional<bool> PrefetchProxyCanaryChecker::CanaryCheckSuccessful() {
  absl::optional<bool> result = LookupAndRunChecksIfNeeded();
  CanaryCheckLookupResult result_enum;
  if (!result.has_value()) {
    result_enum = CanaryCheckLookupResult::kCacheMiss;
  } else if (result.value()) {
    result_enum = CanaryCheckLookupResult::kSuccess;
  } else {
    result_enum = CanaryCheckLookupResult::kFailure;
  }

  base::UmaHistogramEnumeration(AppendNameToHistogram(kCacheLookupResult),
                                result_enum);
  return result;
}

// RunChecksIfNeeded is the public version of LookupAndRunChecksIfNeeded that
// doesn't return the lookup value, to force clients to use
// CanaryCheckSuccessful (which reports UMA) for lookups.
void PrefetchProxyCanaryChecker::RunChecksIfNeeded() {
  LookupAndRunChecksIfNeeded();
}

absl::optional<bool> PrefetchProxyCanaryChecker::LookupAndRunChecksIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Asynchronously update the network cache key. On Android, getting the
  // network cache key can be very slow, so we don't want to block the main
  // thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(GenerateNetworkID, network_connection_tracker_),
      base::BindOnce(&PrefetchProxyCanaryChecker::UpdateCacheKey,
                     weak_factory_.GetWeakPtr()));
  // Assume the cache key has not changed since last time we checked it. Note
  // that if we have never set latest_cache_key_, |it| will be cache_.end().
  auto it = cache_.Get(latest_cache_key_);
  if (it == cache_.end()) {
    SendNowIfInactive();
    return absl::optional<bool>();
  }

  const PrefetchProxyCanaryChecker::CacheEntry& entry = it->second;
  base::TimeDelta cache_entry_age = clock_->Now() - entry.last_modified;

  base::LinearHistogram::FactoryTimeGet(
      AppendNameToHistogram(kCacheEntryAgeHistogram),
      base::Hours(0) /* minimum */, base::Hours(72) /* maximum */,
      50 /* bucket_count */, base::HistogramBase::kUmaTargetedHistogramFlag)
      ->Add(cache_entry_age.InHours());

  // Check if the cache entry should be revalidated because it has expired or
  // cache_entry_age is negative because the clock was moved back.
  if (cache_entry_age >= revalidate_cache_after_ ||
      cache_entry_age.is_negative()) {
    SendNowIfInactive();
  }

  return entry.success;
}

std::string PrefetchProxyCanaryChecker::AppendNameToHistogram(
    const std::string& histogram) const {
  return base::StringPrintf("%s.%s", histogram.c_str(), name_.c_str());
}

void PrefetchProxyCanaryChecker::StartDNSResolution(const GURL& url) {
  net::NetworkAnonymizationKey nak =
      net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url))
          .network_anonymization_key();

  network::mojom::ResolveHostParametersPtr resolve_host_parameters =
      network::mojom::ResolveHostParameters::New();
  resolve_host_parameters->initial_priority = net::RequestPriority::IDLE;
  // Don't use DNS results cached at the user's device.
  resolve_host_parameters->cache_usage =
      network::mojom::ResolveHostParameters::CacheUsage::DISALLOWED;

  // Allow cancelling the request.
  resolver_control_handle_ = mojo::Remote<network::mojom::ResolveHostHandle>();
  resolve_host_parameters->control_handle =
      resolver_control_handle_.BindNewPipeAndPassReceiver();

  mojo::PendingRemote<network::mojom::ResolveHostClient> client_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<PrefetchProxyDNSProber>(
          base::BindOnce(&PrefetchProxyCanaryChecker::OnDNSResolved,
                         weak_factory_.GetWeakPtr())),
      client_remote.InitWithNewPipeAndPassReceiver());

  // TODO(crbug.com/1355169): Consider passing a SchemeHostPort to trigger HTTPS
  // DNS resource record query.
  profile_->GetDefaultStoragePartition()->GetNetworkContext()->ResolveHost(
      network::mojom::HostResolverHost::NewHostPortPair(
          net::HostPortPair::FromURL(url)),
      nak, std::move(resolve_host_parameters), std::move(client_remote));

  timeout_timer_ = std::make_unique<base::OneShotTimer>(tick_clock_);
  // base::Unretained is safe because |timeout_timer_| is owned by this.
  timeout_timer_->Start(
      FROM_HERE, check_timeout_,
      base::BindOnce(&PrefetchProxyCanaryChecker::ProcessTimeout,
                     base::Unretained(this)));
}

void PrefetchProxyCanaryChecker::OnDNSResolved(
    int net_error,
    const absl::optional<net::AddressList>& resolved_addresses) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  timeout_timer_.reset();
  resolver_control_handle_.reset();
  bool successful = net_error == net::OK && resolved_addresses &&
                    !resolved_addresses->empty();
  if (successful) {
    ProcessSuccess();
  } else {
    ProcessFailure(net_error);
  }
}

void PrefetchProxyCanaryChecker::SetNetworkConnectionTrackerForTesting(
    network::NetworkConnectionTracker* tracker) {
  network_connection_tracker_ = tracker;
}
