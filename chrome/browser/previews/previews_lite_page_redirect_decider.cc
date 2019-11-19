// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_redirect_decider.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/previews/previews_ui_tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/previews/content/previews_user_data.h"
#include "components/previews/core/previews_experiments.h"
#include "components/previews/core/previews_features.h"
#include "components/previews/core/previews_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "url/origin.h"

namespace {
const char kUserNeedsNotification[] =
    "previews.litepage.user-needs-notification";
const char kHostBlacklist[] = "previews.litepage.host-blacklist";

const size_t kMaxBlacklistEntries = 30;

// Cleans up the given host blacklist by removing all stale (expiry has passed)
// entries. If after removing all stale entries, the blacklist is still over
// capacity, then remove the entry with the closest expiration.
void RemoveStaleBlacklistEntries(base::DictionaryValue* dict) {
  std::vector<std::string> keys_to_delete;

  base::Time min_value = base::Time::Max();
  std::string min_key;
  for (const auto& iter : dict->DictItems()) {
    base::Time value = base::Time::FromDoubleT(iter.second.GetDouble());

    // Delete all stale entries.
    if (value <= base::Time::Now()) {
      keys_to_delete.push_back(iter.first);
      continue;
    }

    // Record the closest expiration in case we need it later on.
    if (value < min_value) {
      min_value = value;
      min_key = iter.first;
    }
  }

  // Remove all expired entries.
  for (const std::string& key : keys_to_delete)
    dict->RemoveKey(key);

  // Remove the closest expiration if needed.
  if (dict->DictSize() > kMaxBlacklistEntries)
    dict->RemoveKey(min_key);

  DCHECK_GE(kMaxBlacklistEntries, dict->DictSize());
}

}  // namespace

// This WebContentsObserver watches the rest of the current navigation shows a
// notification to the user that this preview now exists and will be used on
// future eligible page loads. This is only done if the navigations finishes on
// the same URL as the one when it began. After finishing the navigation, |this|
// will be removed as an observer.
class UserNotificationWebContentsObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<UserNotificationWebContentsObserver> {
 public:
  void SetUIShownCallback(base::OnceClosure callback) {
    ui_shown_callback_ = std::move(callback);
  }

 private:
  friend class content::WebContentsUserData<
      UserNotificationWebContentsObserver>;

  explicit UserNotificationWebContentsObserver(
      content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  void DestroySelf() {
    content::WebContents* old_web_contents = web_contents();
    Observe(nullptr);
    old_web_contents->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. |this| is destroyed.
  }

  void DidRedirectNavigation(
      content::NavigationHandle* navigation_handle) override {
    DestroySelf();
    // DO NOT add code past this point. |this| is destroyed.
  }

  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (ui_shown_callback_ && handle->GetNetErrorCode() == net::OK) {
      PreviewsLitePageInfoBarDelegate::Create(web_contents());
      std::move(ui_shown_callback_).Run();
    }
    DestroySelf();
    // DO NOT add code past this point. |this| is destroyed.
  }

  base::OnceClosure ui_shown_callback_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNotificationWebContentsObserver)

PreviewsLitePageRedirectDecider::PreviewsLitePageRedirectDecider(
    content::BrowserContext* browser_context)
    : clock_(base::DefaultTickClock::GetInstance()),
      page_id_(base::RandUint64()),
      drp_settings_(nullptr),
      pref_service_(nullptr),
      need_to_show_notification_(false),
      host_bypass_blacklist_(std::make_unique<base::DictionaryValue>()),
      drp_headers_valid_(false),
      browser_context_(browser_context) {
  if (!browser_context)
    return;

  Profile* profile = Profile::FromBrowserContext(browser_context);

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  if (!drp_settings)
    return;

  DCHECK(!browser_context->IsOffTheRecord());

  pref_service_ = profile->GetPrefs();
  host_bypass_blacklist_ =
      pref_service_->GetDictionary(kHostBlacklist)->CreateDeepCopy();

  // Note: This switch has no effect if |drp_settings| was null since
  // |host_bypass_blacklist_| would be empty anyways.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          previews::switches::kClearLitePageRedirectLocalBlacklist)) {
    host_bypass_blacklist_->Clear();
    pref_service_->Set(kHostBlacklist, *host_bypass_blacklist_);
  }

  // Add |this| as an observer to DRP, but if DRP is already initialized, check
  // the prefs now.
  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);
  if (drp_settings_->Config()) {
    OnSettingsInitialized();
    OnProxyRequestHeadersChanged(drp_settings->GetProxyRequestHeaders());
  }

  // This section depends on |pref_service_| being set so it must come after
  // that.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("previews_litepage_prober", R"(
        semantics {
          sender: "Previews Litepage Prober"
          description:
            "Requests a small resource to test network connectivity to a "
            "Google domain."
          trigger:
            "Requested when Lite mode and Previews are enabled at any of the "
            "following events: on startup, on every network change, and every "
            "30 seconds (experiment configurable) to maintain a hot connection."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can control Lite mode on Android via the settings menu. "
            "Lite mode is not available on iOS, and on desktop only for "
            "developer testing."
          policy_exception_justification: "Not implemented."
        })");

  AvailabilityProber::TimeoutPolicy timeout_policy;
  AvailabilityProber::RetryPolicy retry_policy;
  retry_policy.backoff = AvailabilityProber::Backoff::kExponential;
  retry_policy.base_interval = base::TimeDelta::FromSeconds(30);
  retry_policy.use_random_urls = true;

  litepages_service_prober_ = std::make_unique<AvailabilityProber>(
      this, profile->GetURLLoaderFactory(), profile->GetPrefs(),
      AvailabilityProber::ClientName::kLitepages,
      previews::params::LitePageRedirectProbeURL(),
      AvailabilityProber::HttpMethod::kHead, net::HttpRequestHeaders(),
      retry_policy, timeout_policy, traffic_annotation,
      10 /* max_cache_entries */,
      base::TimeDelta::FromHours(24) /* revalidate_cache_after */);
  // Note: probing will only occur when |ShouldSendNextProbe| return true.
  litepages_service_prober_->RepeatedlyProbe(
      previews::params::LitePageRedirectPreviewProbeInterval(),
      true /* send_only_in_foreground */);
}

PreviewsLitePageRedirectDecider::~PreviewsLitePageRedirectDecider() = default;

// static
void PreviewsLitePageRedirectDecider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kUserNeedsNotification, true);
  registry->RegisterDictionaryPref(kHostBlacklist);
}

// static
uint64_t PreviewsLitePageRedirectDecider::GeneratePageIdForWebContents(
    content::WebContents* web_contents) {
  return PreviewsLitePageRedirectDecider::GeneratePageIdForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

// static
uint64_t PreviewsLitePageRedirectDecider::GeneratePageIdForProfile(
    Profile* profile) {
  PreviewsService* previews_service =
      PreviewsServiceFactory::GetForProfile(profile);
  return previews_service
             ? previews_service->previews_lite_page_redirect_decider()
                   ->GeneratePageID()
             : 0;
}

void PreviewsLitePageRedirectDecider::OnProxyRequestHeadersChanged(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string drp_header;
  drp_headers_valid_ =
      headers.GetHeader(data_reduction_proxy::chrome_proxy_header(),
                        &drp_header) &&
      (drp_header.find(",s=") != std::string::npos ||
       drp_header.find(" s=") != std::string::npos ||
       base::StartsWith(drp_header, "s=", base::CompareCase::SENSITIVE));

  // This is done so that successive page ids cannot be used to track users
  // across sessions. These sessions are contained in the chrome-proxy header.
  page_id_ = base::RandUint64();
}

void PreviewsLitePageRedirectDecider::OnSettingsInitialized() {
  // The notification only needs to be shown if the user has never seen it
  // before, and is an existing Data Saver user.
  if (!pref_service_->GetBoolean(kUserNeedsNotification)) {
    need_to_show_notification_ = false;
  } else if (drp_settings_->IsDataReductionProxyEnabled()) {
    need_to_show_notification_ = true;
  } else {
    need_to_show_notification_ = false;
    pref_service_->SetBoolean(kUserNeedsNotification, false);
  }
}

void PreviewsLitePageRedirectDecider::Shutdown() {
  if (drp_settings_)
    drp_settings_->RemoveDataReductionProxySettingsObserver(this);
}

void PreviewsLitePageRedirectDecider::SetClockForTesting(
    const base::TickClock* clock) {
  clock_ = clock;
}

void PreviewsLitePageRedirectDecider::SetDRPSettingsForTesting(
    data_reduction_proxy::DataReductionProxySettings* drp_settings) {
  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);
}

void PreviewsLitePageRedirectDecider::ClearBlacklist() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_bypass_blacklist_->Clear();
  if (pref_service_)
    pref_service_->Set(kHostBlacklist, *host_bypass_blacklist_);
}

void PreviewsLitePageRedirectDecider::ClearStateForTesting() {
  single_bypass_.clear();
  host_bypass_blacklist_->Clear();
}

void PreviewsLitePageRedirectDecider::SetUserHasSeenUINotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  need_to_show_notification_ = false;
  pref_service_->SetBoolean(kUserNeedsNotification, false);
}

void PreviewsLitePageRedirectDecider::SetServerUnavailableFor(
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks retry_at = clock_->NowTicks() + retry_after;
  if (!retry_at_.has_value() || retry_at > retry_at_)
    retry_at_ = retry_at;
}

bool PreviewsLitePageRedirectDecider::IsServerUnavailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!retry_at_.has_value())
    return false;
  bool server_loadshedding = retry_at_ > clock_->NowTicks();
  if (!server_loadshedding)
    retry_at_.reset();
  return server_loadshedding;
}

void PreviewsLitePageRedirectDecider::AddSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Garbage collect any old entries while looking for the one for |url|.
  auto entry = single_bypass_.end();
  for (auto iter = single_bypass_.begin(); iter != single_bypass_.end();
       /* no increment */) {
    if (iter->second < clock_->NowTicks()) {
      iter = single_bypass_.erase(iter);
      continue;
    }
    if (iter->first == url)
      entry = iter;
    ++iter;
  }

  // Update the entry for |url|.
  const base::TimeTicks ttl =
      clock_->NowTicks() + base::TimeDelta::FromMinutes(5);
  if (entry == single_bypass_.end()) {
    single_bypass_.emplace(url, ttl);
    return;
  }
  entry->second = ttl;
}

bool PreviewsLitePageRedirectDecider::CheckSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto entry = single_bypass_.find(url);
  if (entry == single_bypass_.end())
    return false;
  return entry->second >= clock_->NowTicks();
}

uint64_t PreviewsLitePageRedirectDecider::GeneratePageID() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++page_id_;
}

void PreviewsLitePageRedirectDecider::ReportDataSavings(
    int64_t network_bytes,
    int64_t original_bytes,
    const std::string& host) {
  if (!drp_settings_ || !drp_settings_->data_reduction_proxy_service())
    return;

  // The total data usage is tracked for all data in Chrome, so we only need to
  // update the savings.
  int64_t data_saved = original_bytes - network_bytes;
  drp_settings_->data_reduction_proxy_service()->UpdateDataUseForHost(
      0, data_saved, host);

  drp_settings_->data_reduction_proxy_service()->UpdateContentLengths(
      0, data_saved, true /* data_reduction_proxy_enabled */,
      data_reduction_proxy::DataReductionProxyRequestType::
          VIA_DATA_REDUCTION_PROXY,
      "text/html", true /* is_user_traffic */,
      data_use_measurement::DataUseUserData::DataUseContentType::
          MAIN_FRAME_HTML,
      0);
}

bool PreviewsLitePageRedirectDecider::NeedsToNotifyUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          previews::switches::kDoNotRequireLitePageRedirectInfoBar)) {
    return false;
  }
  return need_to_show_notification_;
}

void PreviewsLitePageRedirectDecider::NotifyUser(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(need_to_show_notification_);
  DCHECK(!UserNotificationWebContentsObserver::FromWebContents(web_contents));

  UserNotificationWebContentsObserver::CreateForWebContents(web_contents);
  UserNotificationWebContentsObserver* observer =
      UserNotificationWebContentsObserver::FromWebContents(web_contents);

  // base::Unretained is safe here because |this| outlives |web_contents|.
  observer->SetUIShownCallback(base::BindOnce(
      &PreviewsLitePageRedirectDecider::SetUserHasSeenUINotification,
      base::Unretained(this)));
}

void PreviewsLitePageRedirectDecider::BlacklistBypassedHost(
    const std::string& host,
    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is an existing entry, intentionally update it.
  host_bypass_blacklist_->SetKey(
      host, base::Value((base::Time::Now() + duration).ToDoubleT()));

  RemoveStaleBlacklistEntries(host_bypass_blacklist_.get());
  if (pref_service_)
    pref_service_->Set(kHostBlacklist, *host_bypass_blacklist_);
}

bool PreviewsLitePageRedirectDecider::HostBlacklistedFromBypass(
    const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value* value = host_bypass_blacklist_->FindKey(host);
  if (!value)
    return false;

  DCHECK(value->is_double());
  base::Time expiry = base::Time::FromDoubleT(value->GetDouble());
  return expiry > base::Time::Now();
}

bool PreviewsLitePageRedirectDecider::ShouldSendNextProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_reduction_proxy::DataReductionProxySettings::
             IsDataSaverEnabledByUser(browser_context_->IsOffTheRecord(),
                                      pref_service_) &&
         previews::params::ArePreviewsAllowed() &&
         previews::params::IsLitePageServerPreviewsEnabled() &&
         // Only probe if we rely on it for triggering.
         previews::params::LitePageRedirectOnlyTriggerOnSuccessfulProbe();
}

bool PreviewsLitePageRedirectDecider::IsResponseSuccess(
    net::Error net_error,
    const network::mojom::URLResponseHead* head,
    std::unique_ptr<std::string> body) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Any HTTP response is fine, so long as we got it.
  return net_error == net::OK && head && head->headers;
}

bool PreviewsLitePageRedirectDecider::IsServerProbeResultAvailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return litepages_service_prober_->LastProbeWasSuccessful().has_value();
}

bool PreviewsLitePageRedirectDecider::IsServerReachableByProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Optional<bool> probe =
      litepages_service_prober_->LastProbeWasSuccessful();
  return probe.value_or(false);
}
