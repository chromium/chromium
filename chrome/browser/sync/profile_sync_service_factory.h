// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace browser_sync {
class ChromeSyncClient;
}  // namespace browser_sync

namespace syncer {
class ProfileSyncService;
class SyncService;
}  // namespace syncer

class ProfileSyncServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  using SyncClientFactory =
      base::RepeatingCallback<std::unique_ptr<browser_sync::ChromeSyncClient>(
          Profile*)>;

  // Returns the SyncService for the given profile.
  static syncer::SyncService* GetForProfile(Profile* profile);
  // Returns the ProfileSyncService for the given profile. DO NOT USE unless
  // absolutely necessary! Prefer GetForProfile instead.
  static syncer::ProfileSyncService* GetAsProfileSyncServiceForProfile(
      Profile* profile);

  // Returns whether a SyncService has already been created for the profile.
  // Note that GetForProfile will create the service if it doesn't exist yet.
  static bool HasSyncService(Profile* profile);

  // Checks whether sync is configurable by the user. Returns false if sync is
  // disallowed by the command line or controlled by configuration management.
  // |profile| must not be nullptr.
  static bool IsSyncAllowed(Profile* profile);

  static ProfileSyncServiceFactory* GetInstance();

  // Overrides how the SyncClient is created for testing purposes.
  static void SetSyncClientFactoryForTest(SyncClientFactory* client_factory);

  // Iterates over all profiles that have been loaded so far and extract their
  // SyncService if present. Returned pointers are guaranteed to be not null.
  static std::vector<const syncer::SyncService*> GetAllSyncServices();

 private:
  friend struct base::DefaultSingletonTraits<ProfileSyncServiceFactory>;

  ProfileSyncServiceFactory();
  ~ProfileSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  // A factory function for overriding the way the SyncClient is created.
  // This is a raw pointer so it can be statically initialized.
  static SyncClientFactory* client_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceFactory);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_
