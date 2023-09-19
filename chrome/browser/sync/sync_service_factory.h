// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_SYNC_SERVICE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace syncer {
class SyncServiceImpl;
class SyncService;
}  // namespace syncer

class SyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the SyncService for the given profile.
  static syncer::SyncService* GetForProfile(Profile* profile);
  // Returns the SyncServiceImpl for the given profile. DO NOT USE unless
  // absolutely necessary! Prefer GetForProfile instead.
  static syncer::SyncServiceImpl* GetAsSyncServiceImplForProfileForTesting(
      Profile* profile);

  SyncServiceFactory(const SyncServiceFactory&) = delete;
  SyncServiceFactory& operator=(const SyncServiceFactory&) = delete;

  // Returns whether a SyncService has already been created for the profile.
  // Note that GetForProfile will create the service if it doesn't exist yet.
  static bool HasSyncService(Profile* profile);

  // Checks whether sync is configurable by the user. Returns false if sync is
  // disallowed by the command line or controlled by configuration management.
  // |profile| must not be nullptr.
  static bool IsSyncAllowed(Profile* profile);

  static SyncServiceFactory* GetInstance();

  // Iterates over all profiles that have been loaded so far and extract their
  // SyncService if present. Returned pointers are guaranteed to be not null.
  static std::vector<const syncer::SyncService*> GetAllSyncServices();

  // Returns the default factory, useful in tests where it's null by default.
  static TestingFactory GetDefaultFactory();

 private:
  friend base::NoDestructor<SyncServiceFactory>;

  SyncServiceFactory();
  ~SyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SYNC_SYNC_SERVICE_FACTORY_H_
