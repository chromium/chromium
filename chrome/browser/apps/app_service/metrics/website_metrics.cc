// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/website_metrics.h"

#include <random>

#include "base/containers/contains.h"
#include "base/json/values_util.h"
#include "base/rand_util.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/apps/browser_instance/web_contents_instance_id_utils.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "components/history/core/browser/history_types.h"
#include "components/webapps/browser/banners/installable_web_app_check_result.h"
#include "components/webapps/browser/banners/web_app_banner_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/mojom/installation/installation.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace {

const double mean = 1.0;
const double stddev = 0.025;

std::default_random_engine random_generator(base::RandDouble());
std::normal_distribution<double> distribution(mean, stddev);

// Generate random noise following normal_distribution.
double GetRandomNoise() {
  return distribution(random_generator);
}

// Checks if a given browser is running a windowed app. It will return true for
// web apps, hosted apps, and packaged V1 apps.
bool IsAppBrowser(const Browser* browser) {
  return (browser->is_type_app() || browser->is_type_app_popup()) &&
         !web_app::GetAppIdFromApplicationName(browser->app_name()).empty();
}

aura::Window* GetWindowWithBrowser(Browser* browser) {
  if (!browser) {
    return nullptr;
  }
  BrowserWindow* browser_window = browser->window();
  // In some test cases, browser window might be skipped.
  return browser_window ? browser_window->GetNativeWindow() : nullptr;
}

aura::Window* GetWindowWithTabStripModel(TabStripModel* tab_strip_model) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model() == tab_strip_model) {
      return GetWindowWithBrowser(browser);
    }
  }
  return nullptr;
}

wm::ActivationClient* GetActivationClient(aura::Window* window) {
  if (!window) {
    return nullptr;
  }

  aura::Window* root_window = window->GetRootWindow();
  if (!root_window) {
    return nullptr;
  }

  return wm::GetActivationClient(root_window);
}

bool IsSupportedUrl(const GURL& url) {
  return !url.is_empty() && url.SchemeIsHTTPOrHTTPS();
}

}  // namespace

namespace apps {

constexpr char kWebsiteUsageTime[] = "app_platform_metrics.website_usage_time";
constexpr char kRunningTimeKey[] = "time";
constexpr char kPromotableKey[] = "promotable";

WebsiteMetrics::ActiveTabWebContentsObserver::ActiveTabWebContentsObserver(
    content::WebContents* contents,
    WebsiteMetrics* owner)
    : content::WebContentsObserver(contents), owner_(owner) {}

WebsiteMetrics::ActiveTabWebContentsObserver::~ActiveTabWebContentsObserver() =
    default;

void WebsiteMetrics::ActiveTabWebContentsObserver::OnPrimaryPageChanged() {
  owner_->OnWebContentsUpdated(web_contents());

  if (app_banner_manager_observer_.IsObserving()) {
    return;
  }

  auto* app_banner_manager =
      webapps::AppBannerManager::FromWebContents(web_contents());
  // In some test cases, AppBannerManager might be null.
  if (app_banner_manager) {
    app_banner_manager_observer_.Observe(app_banner_manager);
  }
}

void WebsiteMetrics::ActiveTabWebContentsObserver::PrimaryPageChanged(
    content::Page& page) {
  OnPrimaryPageChanged();
}

void WebsiteMetrics::ActiveTabWebContentsObserver::WebContentsDestroyed() {
  app_banner_manager_observer_.Reset();
}

void WebsiteMetrics::ActiveTabWebContentsObserver::
    OnInstallableWebAppStatusUpdated(
        webapps::InstallableWebAppCheckResult result,
        const std::optional<webapps::WebAppBannerData>& data) {
  owner_->OnInstallableWebAppStatusUpdated(web_contents(), result, data);
}

WebsiteMetrics::UrlInfo::UrlInfo(const base::Value& value) {
  const base::Value::Dict* data_dict = value.GetIfDict();
  if (!data_dict) {
    return;
  }

  std::optional<base::TimeDelta> running_time_value =
      base::ValueToTimeDelta(data_dict->Find(kRunningTimeKey));
  if (!running_time_value.has_value()) {
    return;
  }

  auto promotable_value = data_dict->FindBool(kPromotableKey);
  if (!promotable_value.has_value()) {
    return;
  }

  running_time_in_two_hours = running_time_value.value();
  promotable = promotable_value.value();
}

base::Value::Dict WebsiteMetrics::UrlInfo::ConvertToDict() const {
  base::Value::Dict usage_time_dict;
  usage_time_dict.Set(kRunningTimeKey,
                      base::TimeDeltaToValue(running_time_in_two_hours));
  usage_time_dict.Set(kPromotableKey, promotable);
  return usage_time_dict;
}

WebsiteMetrics::WebsiteMetrics(Profile* profile, int user_type_by_device_type)
    : profile_(profile),
      browser_tab_strip_tracker_(this, nullptr),
      user_type_by_device_type_(user_type_by_device_type) {
  BrowserList::GetInstance()->AddObserver(this);
  browser_tab_strip_tracker_.Init();
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileWithoutCreating(profile);
  if (history_service) {
    history_observation_.Observe(history_service);
  }
}

WebsiteMetrics::~WebsiteMetrics() {
  BrowserList::RemoveObserver(this);

  // Also notify observers.
  for (auto& observer : observers_) {
    observer.OnWebsiteMetricsDestroyed();
  }
}

void WebsiteMetrics::OnBrowserAdded(Browser* browser) {
  if (IsAppBrowser(browser)) {
    return;
  }

  auto* window = GetWindowWithBrowser(browser);
  if (window) {
    observed_windows_.AddObservation(window);
    window_to_web_contents_[window] = nullptr;
  }
}

void WebsiteMetrics::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  DCHECK(tab_strip_model);
  auto* window = GetWindowWithTabStripModel(tab_strip_model);
  if (!window || !base::Contains(window_to_web_contents_, window)) {
    // Skip the app browser window.
    return;
  }

  switch (change.type()) {
    case TabStripModelChange::kInserted:
      OnTabStripModelChangeInsert(window, tab_strip_model, *change.GetInsert(),
                                  selection);
      break;
    case TabStripModelChange::kRemoved:
      OnTabStripModelChangeRemove(window, tab_strip_model, *change.GetRemove(),
                                  selection);
      break;
    case TabStripModelChange::kReplaced:
      OnTabStripModelChangeReplace(*change.GetReplace());
      break;
    case TabStripModelChange::kMoved:
    case TabStripModelChange::kSelectionOnly:
      break;
  }

  if (selection.active_tab_changed()) {
    OnActiveTabChanged(window, selection.old_contents, selection.new_contents);
  }
}

void WebsiteMetrics::OnWindowActivated(ActivationReason reason,
                                       aura::Window* gained_active,
                                       aura::Window* lost_active) {
  SetWindowInActivated(lost_active);
  SetWindowActivated(gained_active);
}

void WebsiteMetrics::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  if (deletion_info.is_from_expiration()) {
    // This is an auto-expiration of history that happens after 90 days. Any
    // data recorded here must be newer than this threshold, so ignore the
    // expiration.
    return;
  }

  // To simplify the implementation, remove all recorded urls no matter whatever
  // `deletion_info`.
  webcontents_to_ukm_key_.clear();
  url_infos_.clear();

  profile_->GetPrefs()->SetDict(kWebsiteUsageTime, base::Value::Dict());
}

void WebsiteMetrics::OnWindowDestroying(aura::Window* window) {
  if (base::Contains(window_to_web_contents_, window)) {
    window_to_web_contents_.erase(window);
  }
  observed_windows_.RemoveObservation(window);
}

void WebsiteMetrics::HistoryServiceBeingDeleted(
    history::HistoryService* history_service) {
  DCHECK(history_observation_.IsObservingSource(history_service));
  history_observation_.Reset();
}

void WebsiteMetrics::OnFiveMinutes() {
  // When the user logs in, there might be usage time for some websites saved in
  // the user pref for the last login, and they haven't been recorded yet. So
  // for the first five minutes, read the usage time saved in the user pref, and
  // record the UKM, then save the new usage time to the user pref.
  if (should_record_ukm_from_pref_) {
    RecordUsageTimeFromPref();
    should_record_ukm_from_pref_ = false;
  }

  SaveUsageTime();
}

void WebsiteMetrics::OnTwoHours() {
  RecordUsageTime();

  std::map<GURL, UrlInfo> url_infos;
  for (const auto& it : webcontents_to_ukm_key_) {
    if (!base::Contains(url_infos, it.second) && !it.second.is_empty() &&
        it.second.SchemeIsHTTPOrHTTPS()) {
      url_infos[it.second] = std::move(url_infos_[it.second]);
    }
  }
  url_infos.swap(url_infos_);
}

void WebsiteMetrics::AddObserver(WebsiteMetrics::Observer* observer) {
  observers_.AddObserver(observer);
}

void WebsiteMetrics::RemoveObserver(WebsiteMetrics::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebsiteMetrics::MaybeObserveWindowActivationClient(aura::Window* window) {
  auto* activation_client = GetActivationClient(window);
  if (!activation_client) {
    return;
  }

  activation_client_to_windows_[activation_client].insert(window);

  if (!activation_client_observations_.IsObservingSource(activation_client)) {
    activation_client_observations_.AddObservation(activation_client);
  }
}

void WebsiteMetrics::MaybeRemoveObserveWindowActivationClient(
    aura::Window* window) {
  auto* activation_client = GetActivationClient(window);
  if (!activation_client) {
    return;
  }

  activation_client_to_windows_[activation_client].erase(window);

  // If there are other windows share the `activation_client`, we can't remove
  // the activation client observation. E.g. for browser windows in Ash, there
  // is only 1 root window, and 1 activation_client. Only when all browser
  // windows will be closed, we can remove the observation.
  if (!activation_client_to_windows_[activation_client].empty()) {
    return;
  }

  activation_client_to_windows_.erase(activation_client);
  if (activation_client_observations_.IsObservingSource(activation_client)) {
    activation_client_observations_.RemoveObservation(activation_client);
  }
}

void WebsiteMetrics::OnTabStripModelChangeInsert(
    aura::Window* window,
    TabStripModel* tab_strip_model,
    const TabStripModelChange::Insert& insert,
    const TabStripSelectionChange& selection) {
  if (insert.contents.size() == 0) {
    return;
  }

  // First tab attached.
  if (tab_strip_model->count() == static_cast<int>(insert.contents.size())) {
    MaybeObserveWindowActivationClient(window);
  }

  for (const auto& inserted_tab : insert.contents) {
    content::WebContents* contents = inserted_tab.contents;
    // The tab is new.
    if (!base::Contains(webcontents_to_observer_map_, contents)) {
      webcontents_to_observer_map_[contents] =
          std::make_unique<WebsiteMetrics::ActiveTabWebContentsObserver>(
              contents, this);
    }
  }
}

void WebsiteMetrics::OnTabStripModelChangeRemove(
    aura::Window* window,
    TabStripModel* tab_strip_model,
    const TabStripModelChange::Remove& remove,
    const TabStripSelectionChange& selection) {
  bool active_tab_removed = false;
  const auto window_it = window_to_web_contents_.find(window);
  for (const auto& removed_tab : remove.contents) {
    ::content::WebContents* const removed_contents = removed_tab.contents;
    OnTabClosed(removed_contents);
    if (window_it != window_to_web_contents_.end() &&
        window_it->second == removed_contents) {
      active_tab_removed = true;
    }
  }

  // Last tab detached.
  if (tab_strip_model->count() == 0) {
    // The browser window will be closed, so remove the window and the web
    // contents.
    if (window_it != window_to_web_contents_.end()) {
      // Only trigger `OnTabClosed` if it has not been already triggered.
      if (!active_tab_removed && window_it->second) {
        OnTabClosed(window_it->second);
      }
      window_to_web_contents_.erase(window_it);
    }
    MaybeRemoveObserveWindowActivationClient(window);
  }
}

void WebsiteMetrics::OnTabStripModelChangeReplace(
    const TabStripModelChange::Replace& replace) {
  OnTabClosed(replace.old_contents);
}

void WebsiteMetrics::OnActiveTabChanged(aura::Window* window,
                                        content::WebContents* old_contents,
                                        content::WebContents* new_contents) {
  if (old_contents) {
    SetTabInActivated(old_contents);

    // Clear `old_contents` from `window_to_web_contents_`.
    auto it = window_to_web_contents_.find(window);
    if (it != window_to_web_contents_.end()) {
      it->second = nullptr;
    }
  }

  if (new_contents) {
    window_to_web_contents_[window] = new_contents;
    // When the tab is drag to a new browser window, PrimaryPageChanged might
    // not be called, so `webcontents_to_ukm_key_` doesn't include
    // `new_contents`. So call PrimaryPageChanged to update web contents and add
    // the website url.
    if (!base::Contains(webcontents_to_ukm_key_, new_contents)) {
      auto it = webcontents_to_observer_map_.find(new_contents);
      if (it != webcontents_to_observer_map_.end()) {
        it->second->OnPrimaryPageChanged();

        auto* app_banner_manager =
            webapps::AppBannerManager::FromWebContents(new_contents);
        // In some test cases, AppBannerManager might be null.
        if (app_banner_manager) {
          it->second->OnInstallableWebAppStatusUpdated(
              app_banner_manager->GetInstallableWebAppCheckResult(),
              app_banner_manager->GetCurrentWebAppBannerData());
        }
      }
      return;
    }
    if (wm::IsActiveWindow(window)) {
      SetTabActivated(new_contents);
    }
  }
}

void WebsiteMetrics::OnTabClosed(content::WebContents* web_contents) {
  SetTabInActivated(web_contents);
  webcontents_to_ukm_key_.erase(web_contents);
  webcontents_to_observer_map_.erase(web_contents);

  // Also notify observers.
  const GURL& url = web_contents->GetVisibleURL();
  for (auto& observer : observers_) {
    observer.OnUrlClosed(url, web_contents);
  }
}

void WebsiteMetrics::OnWebContentsUpdated(content::WebContents* web_contents) {
  // If there is an app for the url, we don't need to record the url, because
  // the app metrics can record the usage time metrics. We need to ensure we
  // notify observers of previous URL being closed if we happen to be tracking
  // it.
  if (GetInstanceAppIdForWebContents(web_contents).has_value()) {
    if (const auto web_contents_it = webcontents_to_ukm_key_.find(web_contents);
        web_contents_it != webcontents_to_ukm_key_.end()) {
      for (auto& observer : observers_) {
        observer.OnUrlClosed(web_contents_it->second, web_contents);
      }
      webcontents_to_ukm_key_.erase(web_contents);
    }
    return;
  }

  auto* const window =
      GetWindowWithBrowser(chrome::FindBrowserWithTab(web_contents));
  if (!window) {
    return;
  }

  // When the primary page of `web_contents` is changed, call SetTabInActivated
  // to calculate the usage time for the previous ukm key url.
  SetTabInActivated(web_contents);
  const GURL& url = web_contents->GetVisibleURL();

  // User could have either opened the URL in a new `WebContents` or navigated
  // from a different URL in a pre-existing `WebContents`. We check for both
  // scenarios and notify observers accordingly.
  const auto web_contents_it = webcontents_to_ukm_key_.find(web_contents);
  if (web_contents_it == webcontents_to_ukm_key_.end() && IsSupportedUrl(url)) {
    // URL opened in a new `WebContent`.
    for (auto& observer : observers_) {
      observer.OnUrlOpened(url, web_contents);
    }
  }
  if (web_contents_it != webcontents_to_ukm_key_.end() &&
      web_contents_it->second != url) {
    // Content navigation in a pre-existing `WebContents`.
    const GURL& previous_url = web_contents_it->second;
    if (IsSupportedUrl(previous_url)) {
      for (auto& observer : observers_) {
        observer.OnUrlClosed(previous_url, web_contents);
      }
    }
    if (IsSupportedUrl(url)) {
      for (auto& observer : observers_) {
        observer.OnUrlOpened(url, web_contents);
      }
    }
  }

  // When the primary page of `web_contents` is changed called by
  // contents::WebContentsObserver::PrimaryPageChanged(), set the visible url as
  // default value for the ukm key url.
  webcontents_to_ukm_key_[web_contents] = url;
  if (!IsSupportedUrl(url)) {
    return;
  }

  auto it = window_to_web_contents_.find(window);
  bool is_activated = wm::IsActiveWindow(window) &&
                      it != window_to_web_contents_.end() &&
                      it->second == web_contents;
  AddUrlInfo(url, web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(),
             base::TimeTicks::Now(), is_activated, /*promotable=*/false);
}

void WebsiteMetrics::OnInstallableWebAppStatusUpdated(
    content::WebContents* web_contents,
    webapps::InstallableWebAppCheckResult result,
    const std::optional<webapps::WebAppBannerData>& data) {
  auto it = webcontents_to_ukm_key_.find(web_contents);
  if (it == webcontents_to_ukm_key_.end()) {
    // If the `web_contents` has been removed or replaced, we don't need to set
    // the url.
    return;
  }
  // WebContents in app windows are filtered out in OnBrowserAdded. Installed
  // web apps opened in tabs are filtered out too. So every WebContents here
  // must be a website not installed.
  if (result == webapps::InstallableWebAppCheckResult::kYes_Promotable) {
    UpdateUrlInfo(it->second, /*promotable=*/true);
  }
}

void WebsiteMetrics::AddUrlInfo(const GURL& url,
                                ukm::SourceId source_id,
                                const base::TimeTicks& start_time,
                                bool is_activated,
                                bool promotable) {
  auto& url_info = url_infos_[url];
  url_info.source_id = source_id;
  url_info.start_time = start_time;
  url_info.is_activated = is_activated;
  url_info.promotable = promotable;
}

void WebsiteMetrics::UpdateUrlInfo(const GURL& url, bool promotable) {
  auto it = url_infos_.find(url);
  if (it != url_infos_.end()) {
    it->second.promotable = promotable;
  }
}

void WebsiteMetrics::SetWindowActivated(aura::Window* window) {
  auto it = window_to_web_contents_.find(window);
  if (it != window_to_web_contents_.end() && it->second) {
    SetTabActivated(it->second);
  }
}

void WebsiteMetrics::SetWindowInActivated(aura::Window* window) {
  auto it = window_to_web_contents_.find(window);
  if (it != window_to_web_contents_.end() && it->second) {
    SetTabInActivated(it->second);
  }
}

void WebsiteMetrics::SetTabActivated(content::WebContents* web_contents) {
  auto web_contents_it = webcontents_to_ukm_key_.find(web_contents);
  if (web_contents_it == webcontents_to_ukm_key_.end()) {
    return;
  }
  auto url_it = url_infos_.find(web_contents_it->second);
  if (url_it == url_infos_.end()) {
    return;
  }
  url_it->second.start_time = base::TimeTicks::Now();
  url_it->second.is_activated = true;
}

void WebsiteMetrics::SetTabInActivated(content::WebContents* web_contents) {
  auto web_contents_it = webcontents_to_ukm_key_.find(web_contents);
  if (web_contents_it == webcontents_to_ukm_key_.end()) {
    return;
  }

  // Check whether `web_contents` is activated. If yes, calculate the running
  // time based on the start time set when `web_contents` is activated.
  auto it = url_infos_.find(web_contents_it->second);
  if (it == url_infos_.end() || !it->second.is_activated) {
    return;
  }

  const auto current_time = base::TimeTicks::Now();
  DCHECK_GE(current_time, it->second.start_time);
  it->second.running_time_in_five_minutes +=
      current_time - it->second.start_time;
  it->second.is_activated = false;
}

void WebsiteMetrics::SaveUsageTime() {
  base::Value::Dict dict;
  for (auto& it : url_infos_) {
    if (it.second.is_activated) {
      // Continued usage of active web content.
      const auto current_time = base::TimeTicks::Now();
      DCHECK_GE(current_time, it.second.start_time);
      it.second.running_time_in_five_minutes +=
          current_time - it.second.start_time;
      it.second.start_time = current_time;
    }

    if (!it.second.running_time_in_five_minutes.is_zero()) {
      // Notify observers before we normalize raw usage data.
      for (auto& observer : observers_) {
        observer.OnUrlUsage(it.first, it.second.running_time_in_five_minutes);
      }

      // Based on the privacy review result, randomly multiply a noise factor to
      // the raw data collected in a 5 minutes slot.
      it.second.running_time_in_two_hours +=
          GetRandomNoise() * it.second.running_time_in_five_minutes;
      it.second.running_time_in_five_minutes = base::TimeDelta();
    }
    // Save all urls running time in the past two hours to the user pref.
    if (!it.second.running_time_in_two_hours.is_zero()) {
      dict.Set(it.first.spec(), it.second.ConvertToDict());
    }
  }

  profile_->GetPrefs()->SetDict(kWebsiteUsageTime, std::move(dict));
}

void WebsiteMetrics::RecordUsageTime() {
  for (auto& it : url_infos_) {
    if (!it.second.running_time_in_two_hours.is_zero()) {
      EmitUkm(it.second.source_id,
              it.second.running_time_in_two_hours.InMilliseconds(),
              it.second.promotable,
              /*is_from_last_login=*/false);
      it.second.running_time_in_two_hours = base::TimeDelta();
    }
  }

  // The app usage time AppKMs have been recorded, so clear the saved usage time
  // in the user pref.
  profile_->GetPrefs()->SetDict(kWebsiteUsageTime, base::Value::Dict());
}

void WebsiteMetrics::RecordUsageTimeFromPref() {
  const base::Value::Dict& usage_time =
      profile_->GetPrefs()->GetDict(kWebsiteUsageTime);

  for (const auto [urlstr, url_info_value] : usage_time) {
    if (urlstr.empty()) {
      continue;
    }
    auto url = GURL(urlstr);
    if (!url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    auto url_info = std::make_unique<UrlInfo>(url_info_value);
    if (!url_info->running_time_in_two_hours.is_zero()) {
      // For the URL records dump from the user pref, since the web_contents
      // doesn't exist due to logout/login, we can't call GetPageUkmSourceId to
      // get the source id with the web_contents. So call
      // GetSourceIdForChromeOSWebsiteURL to generate the UKM source id to log
      // saved URLs from the last login session.
      auto source_id = ukm::UkmRecorder::GetSourceIdForChromeOSWebsiteURL(
          base::PassKey<WebsiteMetrics>(), url);
      EmitUkm(source_id, url_info->running_time_in_two_hours.InMilliseconds(),
              url_info->promotable,
              /*is_from_last_login=*/true);
    }
  }
}

void WebsiteMetrics::EmitUkm(ukm::SourceId source_id,
                             int64_t usage_time,
                             bool promotable,
                             bool is_from_last_login) {
  if (source_id == ukm::kInvalidSourceId) {
    DVLOG(1) << "WebsiteMetrics::EmitUkm source id is invalid.";
    return;
  }

  ukm::builders::ChromeOS_WebsiteUsageTime builder(source_id);
  builder.SetDuration(usage_time)
      .SetIsFromLastLogin(is_from_last_login)
      .SetPromotable(promotable)
      .SetUserDeviceMatrix(user_type_by_device_type_)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace apps
