// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace chromeos {

class PrintJobHistoryService;

// Singleton that owns all PrintJobHistoryServices and associates them with
// Profiles.
class PrintJobHistoryServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the PrintJobHistoryService for |context|, creating it if it is not
  // yet created.
  static PrintJobHistoryService* GetForBrowserContext(
      content::BrowserContext* context);

  static PrintJobHistoryServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<PrintJobHistoryServiceFactory>;

  PrintJobHistoryServiceFactory();
  ~PrintJobHistoryServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PrintJobHistoryServiceFactory);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_HISTORY_PRINT_JOB_HISTORY_SERVICE_FACTORY_H_
