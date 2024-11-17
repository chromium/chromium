// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_

#include <map>
#include <optional>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/values.h"
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
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace webapps {
enum class InstallableWebAppCheckResult;
struct WebAppBannerData;
}  // namespace webapps

namespace apps {

class WebsiteMetricsBrowserTest;
class TestWebsiteMetrics;

extern const char kWebsiteUsageTime[];
extern const char kRunningTimeKey[];
extern const char kUrlContentKey[];
extern const char kPromotableKey[];

// WebsiteMetrics monitors creation/deletion of Browser and its
// TabStripModel to record the website usage time metrics.
class WebsiteMetrics : public BrowserListObserver,
                       public TabStripModelObserver,
                       public aura::WindowObserver,
                       public wm::ActivationChangeObserver,
                       public history::HistoryServiceObserver {
 public:
  // Observer that is notified on certain website events like URL opened, URL
  // closed, etc. Observers are expected to register themselves on session
  // initialization so they do not miss out on events that happen before they
  // are registered.
  class Observer : public base::CheckedObserver {
   public:
    Observer() = default;
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;
    ~Observer() override = default;

    // Invoked when a new URL is opened with specified `WebContents`. We also
    // return the URL that was opened in case there are further updates to
    // `WebContents` forcing a new URL opened event that will follow as a
    // separate notification.
    virtual void OnUrlOpened(const GURL& url_opened,
                             ::content::WebContents* web_contents) {}

    // Invoked when a URL is closed with specified `WebContents`. `WebContents`
    // could reflect current URL in case of content navigation, so we also
    // return the URL that was closed.
    virtual void OnUrlClosed(const GURL& url_closed,
                             ::content::WebContents* web_contents) {}

    // Invoked when URL usage metrics are being recorded (per URL that was used,
    // on a 5 minute interval). `running_time` represents the foreground usage
    // time in the last 5 minute interval. We do not track usage per
    // `WebContents` today. There is a possibility of losing out on initial
    // usage metric records if there are delays in observer registration.
    virtual void OnUrlUsage(const GURL& url, base::TimeDelta running_time) {}

    // Invoked when the `WebsiteMetrics` component (being observed) is being
    // destroyed.
    virtual void OnWebsiteMetricsDestroyed() {}
  };

  WebsiteMetrics(Profile* profile, int user_type_by_device_type);

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

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // history::HistoryServiceObserver:
  void OnHistoryDeletions(history::HistoryService* history_service,
                          const history::DeletionInfo& deletion_info) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // Save the usage time records to the local user perf.
  void OnFiveMinutes();

  // Records the usage time UKM each 2 hours.
  void OnTwoHours();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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

    void OnPrimaryPageChanged();

    // content::WebContentsObserver
    void PrimaryPageChanged(content::Page& page) override;
    void WebContentsDestroyed() override;

    // webapps::AppBannerManager::Observer:
    void OnInstallableWebAppStatusUpdated(
        webapps::InstallableWebAppCheckResult result,
        const std::optional<webapps::WebAppBannerData>& data) override;

   private:
    raw_ptr<WebsiteMetrics> owner_;
    base::ScopedObservation<webapps::AppBannerManager,
                            webapps::AppBannerManager::Observer>
        app_banner_manager_observer_{this};
  };

  struct UrlInfo {
    UrlInfo() = default;
    explicit UrlInfo(const base::Value& value);
    ukm::SourceId source_id = ukm::kInvalidSourceId;
    base::TimeTicks start_time;
    // Running time in the past 5 minutes without noise.
    base::TimeDelta running_time_in_five_minutes;
    // Sum `running_time_in_five_minutes` with noise in the past 2 hours:
    // time1 * noise1 + time2 * noise2 + time3 * noise3....
    base::TimeDelta running_time_in_two_hours;

    bool is_activated = false;
    bool promotable = false;

    // Converts the struct UsageTime to base::Value::Dict, e.g.:
    // {
    //    "time": "3600",
    //    "url_content": "scope",
    //    "promotable": "false",
    // }
    base::Value::Dict ConvertToDict() const;
  };

  // Observes the root window's activation client for the OnWindowActivated
  // callback.
  void MaybeObserveWindowActivationClient(aura::Window* window);

  // Removes observing the root window's activation client when the last browser
  // window is closed.
  void MaybeRemoveObserveWindowActivationClient(aura::Window* window);

  void OnTabStripModelChangeInsert(aura::Window* window,
                                   TabStripModel* tab_strip_model,
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
      content::WebContents* web_contents,
      webapps::InstallableWebAppCheckResult result,
      const std::optional<webapps::WebAppBannerData>& data);

  // Adds the url info to `url_infos_`.
  void AddUrlInfo(const GURL& url,
                  ukm::SourceId source_id,
                  const base::TimeTicks& start_time,
                  bool is_activated,
                  bool promotable);

  // Modifies `url_infos_` to set whether the website can be promoted to PWA,
  // when the website manifest is updated.
  void UpdateUrlInfo(const GURL& old_url, bool promotable);

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

  void EmitUkm(ukm::SourceId source_id,
               int64_t usage_time,
               bool promotable,
               bool is_from_last_login);

  const raw_ptr<Profile> profile_;

  BrowserTabStripTracker browser_tab_strip_tracker_;

  // The map from the window to the active tab contents.
  base::flat_map<aura::Window*, raw_ptr<content::WebContents, CtnExperimental>>
      window_to_web_contents_;

  // The map from the root window's activation client to windows.
  std::map<wm::ActivationClient*,
           std::set<raw_ptr<aura::Window, SetExperimental>>>
      activation_client_to_windows_;

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

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  // For Lacros browser windows, there could be multiple root windows for
  // browser windows, and multiple ActivationClients.
  base::ScopedMultiSourceObservation<wm::ActivationClient,
                                     wm::ActivationChangeObserver>
      activation_client_observations_{this};

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<WebsiteMetrics> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
