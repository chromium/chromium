// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_LOGS_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_LOGS_COLLECTOR_H_

#include <memory>
#include <unordered_map>

#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_contents_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"

class BrowserWindowInterface;

namespace chromeos {

// Collects and observes logs from web content in browser tabs.
class KioskBrowserLogsCollector : public BrowserCollectionObserver {
 public:
  explicit KioskBrowserLogsCollector(
      KioskWebContentsObserver::LoggerCallback logger_callback);
  ~KioskBrowserLogsCollector() override;

  class KioskTabStripModelObserver;

  // `BrowserCollectionObserver` implementation:
  void OnBrowserCreated(BrowserWindowInterface* browser) override;
  void OnBrowserClosed(BrowserWindowInterface* browser) override;

 private:
  void ObserveAlreadyOpenBrowsers();
  void ObserveBrowser(BrowserWindowInterface* browser);

  KioskWebContentsObserver::LoggerCallback logger_callback_;
  std::unordered_map<BrowserWindowInterface*,
                     std::unique_ptr<KioskTabStripModelObserver>>
      tab_strip_model_observers_;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observer_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_BROWSER_LOGS_COLLECTOR_H_
