// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_LOGS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_LOGS_COLLECTOR_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chromeos/ash/components/browser_delegate/browser_controller.h"

namespace content {
class WebContents;
}  // namespace content

namespace ash {
class BrowserDelegate;
}  // namespace ash

namespace chromeos {

// Collects and observes logs from web content in browser tabs.
class KioskBrowserLogsCollector : public ash::BrowserController::TabObserver {
 public:
  explicit KioskBrowserLogsCollector(
      KioskWebContentsObserver::LoggerCallback logger_callback);
  KioskBrowserLogsCollector(const KioskBrowserLogsCollector&) = delete;
  KioskBrowserLogsCollector& operator=(const KioskBrowserLogsCollector&) =
      delete;
  ~KioskBrowserLogsCollector() override;

 private:
  // `ash::BrowserController::TabObserver` implementation:
  void OnTabInserted(ash::BrowserDelegate* browser,
                     content::WebContents* contents) override;
  void OnTabRemoved(ash::BrowserDelegate* browser,
                    content::WebContents* contents,
                    bool will_delete) override;
  void OnTabReplaced(ash::BrowserDelegate* browser,
                     content::WebContents* old_contents,
                     content::WebContents* new_contents) override;

  KioskWebContentsObserver::LoggerCallback logger_callback_;
  base::flat_map<content::WebContents*,
                 std::unique_ptr<KioskWebContentsObserver>>
      web_contents_map_;

  base::ScopedObservation<ash::BrowserController,
                          ash::BrowserController::TabObserver>
      tab_observation_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_LOGS_COLLECTOR_H_
