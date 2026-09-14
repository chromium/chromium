// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_browser_logs_collector.h"

#include <memory>
#include <utility>

#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"
#include "chromeos/ash/components/browser_delegate/browser_delegate.h"
#include "content/public/browser/web_contents.h"

namespace chromeos {

KioskBrowserLogsCollector::KioskBrowserLogsCollector(
    KioskWebContentsObserver::LoggerCallback logger_callback)
    : logger_callback_(std::move(logger_callback)) {
  tab_observation_.Observe(ash::BrowserController::GetInstance());
  ash::BrowserController::GetInstance()->ForEachBrowser(
      ash::BrowserController::BrowserOrder::kAscendingCreationTime,
      [&](ash::BrowserDelegate& browser) {
        for (tabs::TabInterface* tab : browser.GetTabIterator()) {
          OnTabInserted(&browser, tab->GetContents());
        }
        return ash::BrowserController::kContinueIteration;
      });
}

KioskBrowserLogsCollector::~KioskBrowserLogsCollector() = default;

void KioskBrowserLogsCollector::OnTabInserted(ash::BrowserDelegate* browser,
                                              content::WebContents* contents) {
  if (!web_contents_map_.contains(contents)) {
    web_contents_map_.emplace(
        contents,
        std::make_unique<KioskWebContentsObserver>(contents, logger_callback_));
  }
}

void KioskBrowserLogsCollector::OnTabRemoved(ash::BrowserDelegate* browser,
                                             content::WebContents* contents,
                                             bool will_delete) {
  web_contents_map_.erase(contents);
}

void KioskBrowserLogsCollector::OnTabReplaced(
    ash::BrowserDelegate* browser,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  OnTabRemoved(browser, old_contents, /*will_delete=*/false);
  OnTabInserted(browser, new_contents);
}

}  // namespace chromeos
