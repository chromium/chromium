// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"
#include "url/gurl.h"

class Browser;
class Profile;

namespace webapps {
struct InstallableData;
}

namespace apps {

class WebsiteMetricsBrowserTest;

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

 private:
  friend class WebsiteMetricsBrowserTest;

  // This class monitors the activated WebContent for the activated browser
  // window and notifies a navigation to the WebsiteMetrics.
  class ActiveTabWebContentsObserver : public content::WebContentsObserver {
   public:
    ActiveTabWebContentsObserver(content::WebContents* contents,
                                 WebsiteMetrics* owner);

    ActiveTabWebContentsObserver(const ActiveTabWebContentsObserver&) = delete;
    ActiveTabWebContentsObserver& operator=(
        const ActiveTabWebContentsObserver&) = delete;

    ~ActiveTabWebContentsObserver() override = default;

    // content::WebContentsObserver
    void PrimaryPageChanged(content::Page& page) override;

   private:
    WebsiteMetrics* owner_;
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

  // Called by |WebsiteMetrics::ActiveTabWebContentsObserver|.
  void OnWebContentsUpdated(content::WebContents* contents);

  // Callback invoked by the InstallableManager once it has finished checking
  // all other installable properties.
  void OnDidPerformInstallableWebAppCheck(content::WebContents* web_contents,
                                          const webapps::InstallableData& data);

  BrowserTabStripTracker browser_tab_strip_tracker_;

  // The map from the window to the active tab contents.
  base::flat_map<aura::Window*, content::WebContents*> window_to_web_contents_;

  base::flat_map<content::WebContents*,
                 std::unique_ptr<ActiveTabWebContentsObserver>>
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
