// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_logs_collector.h"

#include <memory>
#include <unordered_map>

#include "base/functional/bind.h"
#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

class KioskBrowserLogsCollector::KioskTabStripModelObserver
    : public TabStripModelObserver {
 public:
  KioskTabStripModelObserver(
      TabStripModel* tab_strip_model,
      KioskWebContentsObserver::LoggerCallback logger_callback)
      : logger_callback_(logger_callback) {
    tab_strip_model_observer_.Observe(tab_strip_model);
    ObserveWebContentsFromTabStripModel(tab_strip_model);
  }

  // `TabStripModelObserver` implementation:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    switch (change.type()) {
      case TabStripModelChange::kInserted: {
        const TabStripModelChange::Insert* insert = change.GetInsert();
        if (insert == nullptr) {
          return;
        }

        for (const TabStripModelChange::ContentsWithIndex& content_with_index :
             insert->contents) {
          ObserveWebContents(content_with_index.contents);
        }
        break;
      }
      case TabStripModelChange::kRemoved: {
        for (const TabStripModelChange::RemovedTab& removed_tab :
             change.GetRemove()->contents) {
          StopObservingWebContents(removed_tab.contents);
        }
        break;
      }
      case TabStripModelChange::kReplaced: {
        const TabStripModelChange::Replace* replace = change.GetReplace();
        if (replace == nullptr) {
          return;
        }

        StopObservingWebContents(replace->old_contents);
        ObserveWebContents(replace->new_contents);
        break;
      }
      case TabStripModelChange::kSelectionOnly:
      case TabStripModelChange::kMoved:
        // Not need to be handled as the web contents are not updated.
        break;
    }
  }

 private:
  void ObserveWebContentsFromTabStripModel(TabStripModel* tab_strip_model) {
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      ObserveWebContents(tab_strip_model->GetWebContentsAt(i));
    }
  }

  void ObserveWebContents(content::WebContents* web_contents) {
    if (!web_contents) {
      return;
    }
    if (web_contents_map_.contains(web_contents)) {
      return;
    }

    web_contents_map_.emplace(
        web_contents, std::make_unique<KioskWebContentsObserver>(
                          web_contents, base::BindRepeating(logger_callback_)));
  }

  void StopObservingWebContents(content::WebContents* web_contents) {
    if (!web_contents) {
      return;
    }

    web_contents_map_.erase(web_contents);
  }

  KioskWebContentsObserver::LoggerCallback logger_callback_;
  std::unordered_map<content::WebContents*,
                     std::unique_ptr<KioskWebContentsObserver>>
      web_contents_map_;

  base::ScopedObservation<TabStripModel, KioskTabStripModelObserver>
      tab_strip_model_observer_{this};
};

KioskBrowserLogsCollector::KioskBrowserLogsCollector(
    KioskWebContentsObserver::LoggerCallback logger_callback)
    : logger_callback_(logger_callback) {
  if (!BrowserList::GetInstance()) {
    LOG(ERROR)
        << "BrowserList is not initialised hence,not collecting browser logs";
    return;
  }

  browser_list_observer_.Observe(BrowserList::GetInstance());
  ObserveAlreadyOpenBrowsers();
}

KioskBrowserLogsCollector::~KioskBrowserLogsCollector() = default;

void KioskBrowserLogsCollector::OnBrowserAdded(Browser* browser) {
  ObserveBrowser(browser);
}

void KioskBrowserLogsCollector::OnBrowserRemoved(Browser* browser) {
  if (!browser) {
    return;
  }

  tab_strip_model_observers_.erase(browser);
}

void KioskBrowserLogsCollector::ObserveAlreadyOpenBrowsers() {
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [&](BrowserWindowInterface* browser) {
        ObserveBrowser(browser);
        return true;
      });
}

void KioskBrowserLogsCollector::ObserveBrowser(
    BrowserWindowInterface* browser) {
  if (!browser || !browser->GetTabStripModel() ||
      tab_strip_model_observers_.contains(browser)) {
    return;
  }

  tab_strip_model_observers_.emplace(
      browser,
      std::make_unique<KioskTabStripModelObserver>(
          browser->GetTabStripModel(), base::BindRepeating(logger_callback_)));
}

}  // namespace chromeos
