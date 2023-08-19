// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EARLY_PREFS_EARLY_PREFS_EXPORT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_EARLY_PREFS_EARLY_PREFS_EXPORT_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/early_prefs/early_prefs_export_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class EarlyPrefsExportServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static EarlyPrefsExportServiceFactory* GetInstance();

  EarlyPrefsExportServiceFactory(const EarlyPrefsExportServiceFactory&) =
      delete;
  EarlyPrefsExportServiceFactory& operator=(
      const EarlyPrefsExportServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<EarlyPrefsExportServiceFactory>;

  EarlyPrefsExportServiceFactory();
  ~EarlyPrefsExportServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_EARLY_PREFS_EARLY_PREFS_EXPORT_SERVICE_FACTORY_H_
