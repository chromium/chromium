// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LIST_PREFS_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LIST_PREFS_FACTORY_H_

#include <memory>
#include <unordered_map>

#include "ash/components/arc/mojom/app.mojom-forward.h"
#include "ash/components/arc/session/connection_holder.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class ArcAppListPrefs;

class ArcAppListPrefsFactory : public ProfileKeyedServiceFactory {
 public:
  static ArcAppListPrefs* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcAppListPrefsFactory* GetInstance();

  static void SetFactoryForSyncTest();
  static bool IsFactorySetForSyncTest();
  void RecreateServiceInstanceForTesting(content::BrowserContext* context);

 private:
  friend base::NoDestructor<ArcAppListPrefsFactory>;

  ArcAppListPrefsFactory();
  ArcAppListPrefsFactory(const ArcAppListPrefsFactory&) = delete;
  ArcAppListPrefsFactory& operator=(const ArcAppListPrefsFactory&) = delete;
  ~ArcAppListPrefsFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  static bool is_sync_test_;

  mutable std::unordered_map<
      content::BrowserContext*,
      std::unique_ptr<
          arc::ConnectionHolder<arc::mojom::AppInstance, arc::mojom::AppHost>>>
      sync_test_app_connection_holders_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_LIST_PREFS_FACTORY_H_
