// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/previews/previews_lite_page_decider.h"

#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/previews/previews_lite_page_infobar_delegate.h"
#include "chrome/browser/previews/previews_lite_page_navigation_throttle.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_experiments.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "net/base/net_errors.h"

namespace {
const char kUserNeedsNotification[] =
    "previews.litepage.user-needs-notification";
const char kHostBlacklist[] = "previews.litepage.host-blacklist";

const size_t kMaxBlacklistEntries = 30;

// Cleans up the given host blacklist by removing all stale (expiry has passed)
// entries. If after removing all stale entries, the blacklist is still over
// capacity, then remove the entry with the closest expiration.
void RemoveStaleEntries(base::DictionaryValue* dict) {
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
};

PreviewsLitePageDecider::PreviewsLitePageDecider(
    content::BrowserContext* browser_context)
    : clock_(base::DefaultTickClock::GetInstance()),
      page_id_(base::RandUint64()),
      drp_settings_(nullptr),
      pref_service_(nullptr),
      host_blacklist_(std::make_unique<base::DictionaryValue>()) {
  if (!browser_context)
    return;

  DataReductionProxyChromeSettings* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
          browser_context);
  if (!drp_settings)
    return;

  DCHECK(!browser_context->IsOffTheRecord());

  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);

  Profile* profile = Profile::FromBrowserContext(browser_context);
  pref_service_ = profile->GetPrefs();
  DCHECK(pref_service_);

  host_blacklist_ =
      pref_service_->GetDictionary(kHostBlacklist)->CreateDeepCopy();
}

PreviewsLitePageDecider::~PreviewsLitePageDecider() = default;

// static
void PreviewsLitePageDecider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kUserNeedsNotification, true);
  registry->RegisterDictionaryPref(kHostBlacklist,
                                   std::make_unique<base::DictionaryValue>());
}

// static
std::unique_ptr<content::NavigationThrottle>
PreviewsLitePageDecider::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  DCHECK(handle);
  DCHECK(handle->GetWebContents());
  DCHECK(handle->GetWebContents()->GetBrowserContext());

  content::BrowserContext* browser_context =
      handle->GetWebContents()->GetBrowserContext();

  PreviewsService* previews_service = PreviewsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!previews_service)
    return nullptr;
  DCHECK(!browser_context->IsOffTheRecord());

  PreviewsLitePageDecider* decider =
      previews_service->previews_lite_page_decider();
  DCHECK(decider);

  // TODO(crbug/842233): Replace this logic with PreviewsState.
  bool drp_enabled = decider->drp_settings_->IsDataReductionProxyEnabled();
  bool preview_enabled = previews::params::ArePreviewsAllowed() &&
                         previews::params::IsLitePageServerPreviewsEnabled();

  if (drp_enabled && preview_enabled) {
    return std::make_unique<PreviewsLitePageNavigationThrottle>(handle,
                                                                decider);
  }
  return nullptr;
}

void PreviewsLitePageDecider::OnProxyRequestHeadersChanged(
    const net::HttpRequestHeaders& headers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This is done so that successive page ids cannot be used to track users
  // across sessions. These sessions are contained in the chrome-proxy header.
  page_id_ = base::RandUint64();
}

void PreviewsLitePageDecider::OnSettingsInitialized() {
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

void PreviewsLitePageDecider::Shutdown() {
  if (drp_settings_)
    drp_settings_->RemoveDataReductionProxySettingsObserver(this);
}

void PreviewsLitePageDecider::SetClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

void PreviewsLitePageDecider::SetDRPSettingsForTesting(
    data_reduction_proxy::DataReductionProxySettings* drp_settings) {
  drp_settings_ = drp_settings;
  drp_settings_->AddDataReductionProxySettingsObserver(this);
}

void PreviewsLitePageDecider::ClearBlacklist() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  host_blacklist_->Clear();
  if (pref_service_)
    pref_service_->Set(kHostBlacklist, *host_blacklist_);
}

void PreviewsLitePageDecider::ClearStateForTesting() {
  single_bypass_.clear();
  host_blacklist_->Clear();
}

void PreviewsLitePageDecider::SetUserHasSeenUINotification() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pref_service_);
  need_to_show_notification_ = false;
  pref_service_->SetBoolean(kUserNeedsNotification, false);
}

void PreviewsLitePageDecider::SetServerUnavailableFor(
    base::TimeDelta retry_after) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::TimeTicks retry_at = clock_->NowTicks() + retry_after;
  if (!retry_at_.has_value() || retry_at > retry_at_)
    retry_at_ = retry_at;
}

bool PreviewsLitePageDecider::IsServerUnavailable() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!retry_at_.has_value())
    return false;
  bool server_loadshedding = retry_at_ > clock_->NowTicks();
  if (!server_loadshedding)
    retry_at_.reset();
  return server_loadshedding;
}

void PreviewsLitePageDecider::AddSingleBypass(std::string url) {
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

bool PreviewsLitePageDecider::CheckSingleBypass(std::string url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto entry = single_bypass_.find(url);
  if (entry == single_bypass_.end())
    return false;
  return entry->second >= clock_->NowTicks();
}

uint64_t PreviewsLitePageDecider::GeneratePageID() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++page_id_;
}

void PreviewsLitePageDecider::ReportDataSavings(int64_t network_bytes,
                                                int64_t original_bytes,
                                                const std::string& host) {
  if (!drp_settings_ || !drp_settings_->data_reduction_proxy_service())
    return;

  drp_settings_->data_reduction_proxy_service()->UpdateDataUseForHost(
      network_bytes, original_bytes, host);

  drp_settings_->data_reduction_proxy_service()->UpdateContentLengths(
      network_bytes, original_bytes, true /* data_reduction_proxy_enabled */,
      data_reduction_proxy::DataReductionProxyRequestType::
          VIA_DATA_REDUCTION_PROXY,
      "text/html", true /* is_user_traffic */,
      data_use_measurement::DataUseUserData::DataUseContentType::
          MAIN_FRAME_HTML,
      0);
}

bool PreviewsLitePageDecider::NeedsToNotifyUser() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return need_to_show_notification_;
}

void PreviewsLitePageDecider::NotifyUser(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(need_to_show_notification_);
  DCHECK(!UserNotificationWebContentsObserver::FromWebContents(web_contents));

  UserNotificationWebContentsObserver::CreateForWebContents(web_contents);
  UserNotificationWebContentsObserver* observer =
      UserNotificationWebContentsObserver::FromWebContents(web_contents);

  // base::Unretained is safe here because |this| outlives |web_contents|.
  observer->SetUIShownCallback(
      base::BindOnce(&PreviewsLitePageDecider::SetUserHasSeenUINotification,
                     base::Unretained(this)));
}

void PreviewsLitePageDecider::BlacklistHost(const std::string& host,
                                            base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If there is an existing entry, intentionally update it.
  host_blacklist_->SetKey(
      host, base::Value((base::Time::Now() + duration).ToDoubleT()));

  RemoveStaleEntries(host_blacklist_.get());
  if (pref_service_)
    pref_service_->Set(kHostBlacklist, *host_blacklist_);
}

bool PreviewsLitePageDecider::HostBlacklisted(const std::string& host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value* value = host_blacklist_->FindKey(host);
  if (!value)
    return false;

  DCHECK(value->is_double());
  base::Time expiry = base::Time::FromDoubleT(value->GetDouble());
  return expiry > base::Time::Now();
}
