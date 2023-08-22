// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_ERROR_NOTIFIER_FACTORY_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_ERROR_NOTIFIER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

class SyncErrorNotifier;

// Singleton that owns all SyncErrorNotifiers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated SyncErrorNotifier.
class SyncErrorNotifierFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of SyncErrorNotifier associated with this profile,
  // creating one if none exists and the shell exists.
  static SyncErrorNotifier* GetForProfile(Profile* profile);

  // Returns an instance of the SyncErrorNotifierFactory singleton.
  static SyncErrorNotifierFactory* GetInstance();

  SyncErrorNotifierFactory(const SyncErrorNotifierFactory&) = delete;
  SyncErrorNotifierFactory& operator=(const SyncErrorNotifierFactory&) = delete;

 private:
  friend base::NoDestructor<SyncErrorNotifierFactory>;

  SyncErrorNotifierFactory();
  ~SyncErrorNotifierFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_ERROR_NOTIFIER_FACTORY_H_
