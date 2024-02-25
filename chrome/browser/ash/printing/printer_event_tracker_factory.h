// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {

class PrinterEventTracker;

class PrinterEventTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  static PrinterEventTrackerFactory* GetInstance();
  static PrinterEventTracker* GetForBrowserContext(
      content::BrowserContext* browser_context);

  PrinterEventTrackerFactory();

  PrinterEventTrackerFactory(const PrinterEventTrackerFactory&) = delete;
  PrinterEventTrackerFactory& operator=(const PrinterEventTrackerFactory&) =
      delete;

  ~PrinterEventTrackerFactory() override;

  // Enables/Disables logging for all trackers. Trackers created in the future
  // are created with the current logging state. Logging is enabled if |enabled|
  // is true.
  void SetLogging(bool enabled);

 private:
  bool logging_enabled_ = true;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_
