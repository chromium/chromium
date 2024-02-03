// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_PAGE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/profile_based_browsing_history_driver.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_resumption/tab_resumption.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace sync_sessions {
class OpenTabsUIDelegate;
}

// The handler for communication between the WebUI
class TabResumptionPageHandler
    : public ntp::tab_resumption::mojom::PageHandler {
 public:
  TabResumptionPageHandler(
      mojo::PendingReceiver<ntp::tab_resumption::mojom::PageHandler>
          pending_page_handler,
      content::WebContents* web_contents);

  TabResumptionPageHandler(const TabResumptionPageHandler&) = delete;
  TabResumptionPageHandler& operator=(const TabResumptionPageHandler&) = delete;

  ~TabResumptionPageHandler() override;

  // tab_resumption::mojom::PageHandler:
  void GetTabs(GetTabsCallback callback) override;

  sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate();

  std::vector<history::mojom::TabPtr> GetForeignTabs();

  // Callback to return annotated visits for a set of url results.
  void OnQueryURLsComplete(std::vector<history::mojom::TabPtr> tabs,
                           GetTabsCallback callback,
                           std::vector<history::QueryURLResult> results);

  // Callback to return the tabs from the associated annotated visits based
  // on if their visibility scores pass a visibility threshold.
  void OnAnnotatedVisits(
      std::vector<history::mojom::TabPtr> tabs,
      GetTabsCallback callback,
      const std::vector<history::AnnotatedVisit> annotated_visits);

 private:
  // The task tracker for the HistoryService callbacks.
  base::CancelableTaskTracker task_tracker_;

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  mojo::Receiver<ntp::tab_resumption::mojom::PageHandler> page_handler_;

  const float visibility_threshold_;

  base::WeakPtrFactory<TabResumptionPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_TAB_RESUMPTION_TAB_RESUMPTION_PAGE_HANDLER_H_
