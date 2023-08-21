// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class AppSyncUIState;
class Profile;

// Singleton that owns all AppSyncUIStates and associates them with profiles.
class AppSyncUIStateFactory : public ProfileKeyedServiceFactory {
 public:
  AppSyncUIStateFactory(const AppSyncUIStateFactory&) = delete;
  AppSyncUIStateFactory& operator=(const AppSyncUIStateFactory&) = delete;

  static AppSyncUIState* GetForProfile(Profile* profile);

  static AppSyncUIStateFactory* GetInstance();

 private:
  friend base::NoDestructor<AppSyncUIStateFactory>;

  AppSyncUIStateFactory();
  ~AppSyncUIStateFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_APP_SYNC_UI_STATE_FACTORY_H_
