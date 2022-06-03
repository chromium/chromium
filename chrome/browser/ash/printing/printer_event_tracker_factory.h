// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {

class PrinterEventTracker;

class PrinterEventTrackerFactory : public BrowserContextKeyedServiceFactory {
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
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when ChromeOS code migration is done.
namespace chromeos {
using ::ash::PrinterEventTrackerFactory;
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTER_EVENT_TRACKER_FACTORY_H_
