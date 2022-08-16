// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_

#include <map>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "components/webapps/browser/banners/app_banner_manager.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace apps {

class WebsiteMetricsBrowserTest;
class TestWebsiteMetrics;

// This is used for logging, so do not remove or reorder existing entries.
enum class UrlContent {
  kUnknown = 0,
  kFullUrl = 1,
  kScope = 2,

  // Add any new values above this one, and update kMaxValue to the highest
  // enumerator value.
  kMaxValue = kScope,
};

extern const char kWebsiteUsageTime[];
extern const char kRunningTimeKey[];
extern const char kUrlContentKey[];
extern const char kPromotableKey[];

// WebsiteMetrics monitors creation/deletion of Browser and its
// TabStripModel to record the website usage time metrics.
class WebsiteMetrics : public BrowserListObserver,
                       public TabStripModelObserver,
                       public wm::ActivationChangeObserver,
                       public history::HistoryServiceObserver {
 public:
  explicit WebsiteMetrics(Profile* profile);

  WebsiteMetrics(const WebsiteMetrics&) = delete;
  WebsiteMetrics& operator=(const WebsiteMetrics&) = delete;

  ~WebsiteMetrics() override;

  // BrowserListObserver overrides:
  void OnBrowserAdded(Browser* browser) override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // history::HistoryServiceObserver:
  void OnURLsDeleted(history::HistoryService* history_service,
                     const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Save the usage time records to the local user perf.
  void OnFiveMinutes();

  // Records the usage time UKM each 2 hours.
  void OnTwoHours();

 private:
  friend class WebsiteMetricsBrowserTest;
  friend class TestWebsiteMetrics;

  // This class monitors the activated WebContent for the activated browser
  // window and notifies a navigation to the WebsiteMetrics.
  class ActiveTabWebContentsObserver
      : public content::WebContentsObserver,
        public webapps::AppBannerManager::Observer {
   public:
    ActiveTabWebContentsObserver(content::WebContents* contents,
                                 WebsiteMetrics* owner);

    ActiveTabWebContentsObserver(const ActiveTabWebContentsObserver&) = delete;
    ActiveTabWebContentsObserver& operator=(
        const ActiveTabWebContentsObserver&) = delete;

    ~ActiveTabWebContentsObserver() override;

    // content::WebContentsObserver
    void PrimaryPageChanged(content::Page& page) override;
    void WebContentsDestroyed() override;

    // webapps::AppBannerManager::Observer:
    void OnInstallableWebAppStatusUpdated() override;

   private:
    WebsiteMetrics* owner_;
    base::ScopedObservation<webapps::AppBannerManager,
                            webapps::AppBannerManager::Observer>
        app_banner_manager_observer_{this};
  };

  struct UrlInfo {
    UrlInfo() = default;
    explicit UrlInfo(const base::Value& value);
    base::TimeTicks start_time;
    // Running time in the past 5 minutes without noise.
    base::TimeDelta running_time_in_five_minutes;
    // Sum `running_time_in_five_minutes` with noise in the past 2 hours:
    // time1 * noise1 + time2 * noise2 + time3 * noise3....
    base::TimeDelta running_time_in_two_hours;

    UrlContent url_content = UrlContent::kUnknown;
    bool is_activated = false;
    bool promotable = false;

    // Converts the struct UsageTime to base::Value, e.g.:
    // {
    //    "time": "3600",
    //    "url_content": "scope",
    //    "promotable": "false",
    // }
    base::Value ConvertToValue() const;
  };

  void OnTabStripModelChangeInsert(TabStripModel* tab_strip_model,
                                   const TabStripModelChange::Insert& insert,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeRemove(aura::Window* window,
                                   TabStripModel* tab_strip_model,
                                   const TabStripModelChange::Remove& remove,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeReplace(
      const TabStripModelChange::Replace& replace);
  void OnActiveTabChanged(aura::Window* window,
                          content::WebContents* old_contents,
                          content::WebContents* new_contents);
  void OnTabClosed(content::WebContents* web_contents);

  // Called by |WebsiteMetrics::ActiveTabWebContentsObserver|.
  virtual void OnWebContentsUpdated(content::WebContents* web_contents);
  virtual void OnInstallableWebAppStatusUpdated(
      content::WebContents* web_contents);

  // Adds the url info to `url_infos_`.
  void AddUrlInfo(const GURL& url,
                  const base::TimeTicks& start_time,
                  UrlContent url_content,
                  bool is_activated,
                  bool promotable);

  // Modifies `old_url` to `new_url` in `url_infos_`, when the website manifest
  // is updated.
  void UpdateUrlInfo(const GURL& old_url,
                     const GURL& new_url,
                     UrlContent url_content,
                     bool is_activated,
                     bool promotable);

  void SetWindowActivated(aura::Window* window);

  void SetWindowInActivated(aura::Window* window);

  void SetTabActivated(content::WebContents* web_contents);

  void SetTabInActivated(content::WebContents* web_contents);

  // Saves the website usage time in `url_infos_` to the user pref each 5
  // minutes.
  void SaveUsageTime();

  // Records the website usage time metrics each 2 hours.
  void RecordUsageTime();

  // Records the usage time UKM saved in the user pref at the first 5 minutes
  // after the user logs in.
  void RecordUsageTimeFromPref();

  void EmitUkm(const GURL& url,
               int64_t usage_time,
               UrlContent url_content,
               bool promotable,
               bool is_from_last_login);

  const raw_ptr<Profile> profile_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  // The map from the window to the active tab contents.
  base::flat_map<aura::Window*, content::WebContents*> window_to_web_contents_;

  std::map<content::WebContents*, std::unique_ptr<ActiveTabWebContentsObserver>>
      webcontents_to_observer_map_;

  // The map from the web_contents to the ukm key url. When the url for web
  // contents is updated in OnWebContentsUpdated, we can get the previous url
  // from this map to calculate the usage time for the previous url.
  //
  // If the url is used for an app, it won't be added to the map, because the
  // app metrics can record the usage time metrics.
  //
  // If the website has a manifest, we might use the scope or the start url as
  // the ukm key url. Otherwise, the visible url is used as the ukm key url.
  std::map<content::WebContents*, GURL> webcontents_to_ukm_key_;

  // Saves the usage info for the activated urls in activated windows for the
  // UKM records. `url_infos_` is cleared after recording the UKM each 2 hours.
  std::map<GURL, UrlInfo> url_infos_;

  int user_type_by_device_type_ = 0;

  bool should_record_ukm_from_pref_ = true;

  // A set of observed activation clients for all browser's windows.
  base::ScopedMultiSourceObservation<wm::ActivationClient,
                                     wm::ActivationChangeObserver>
      activation_client_observations_{this};

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};

  base::WeakPtrFactory<WebsiteMetrics> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
