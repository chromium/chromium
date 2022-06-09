// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_

#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_client.h"

class Browser;
class Profile;

namespace apps {

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
  void OnBrowserRemoved(Browser* browser) override;

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
  void OnTabStripModelChangeInsert(TabStripModel* tab_strip_model,
                                   const TabStripModelChange::Insert& insert,
                                   const TabStripSelectionChange& selection);
  void OnTabStripModelChangeRemove(TabStripModel* tab_strip_model,
                                   const TabStripModelChange::Remove& remove,
                                   const TabStripSelectionChange& selection);

  BrowserTabStripTracker browser_tab_strip_tracker_;

  // The map from the window to the active tab contents.
  base::flat_map<aura::Window*, content::WebContents*> window_to_web_contents_;

  // A set of observed activation clients for all browser's windows.
  base::ScopedMultiSourceObservation<wm::ActivationClient,
                                     wm::ActivationChangeObserver>
      activation_client_observations_{this};

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_observation_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_METRICS_WEBSITE_METRICS_H_
