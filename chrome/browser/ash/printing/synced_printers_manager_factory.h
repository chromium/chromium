// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_SYNCED_PRINTERS_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PRINTING_SYNCED_PRINTERS_MANAGER_FACTORY_H_

#include "base/lazy_instance.h"
#include "chrome/browser/ash/printing/synced_printers_manager.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {

class SyncedPrintersManager;

class SyncedPrintersManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static SyncedPrintersManager* GetForBrowserContext(
      content::BrowserContext* context);

  static SyncedPrintersManagerFactory* GetInstance();

  SyncedPrintersManagerFactory(const SyncedPrintersManagerFactory&) = delete;
  SyncedPrintersManagerFactory& operator=(const SyncedPrintersManagerFactory&) =
      delete;

 private:
  friend struct base::LazyInstanceTraitsBase<SyncedPrintersManagerFactory>;

  SyncedPrintersManagerFactory();
  ~SyncedPrintersManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  SyncedPrintersManager* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_SYNCED_PRINTERS_MANAGER_FACTORY_H_
