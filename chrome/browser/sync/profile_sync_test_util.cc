// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_test_util.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/invalidation/deprecated_profile_invalidation_provider_factory.h"
#include "chrome/browser/invalidation/profile_invalidation_provider_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/chrome_sync_client.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browser_sync/profile_sync_test_util.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/invalidation/impl/profile_invalidation_provider.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/driver/startup_controller.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

using browser_sync::ProfileSyncService;
using testing::NiceMock;

ProfileSyncService::InitParams CreateProfileSyncServiceParamsForTest(
    Profile* profile) {
  auto sync_client = std::make_unique<browser_sync::ChromeSyncClient>(profile);

  sync_client->SetSyncApiComponentFactoryForTesting(
      std::make_unique<NiceMock<syncer::SyncApiComponentFactoryMock>>());

  ProfileSyncService::InitParams init_params =
      CreateProfileSyncServiceParamsForTest(std::move(sync_client), profile);

  return init_params;
}

ProfileSyncService::InitParams CreateProfileSyncServiceParamsForTest(
    std::unique_ptr<syncer::SyncClient> sync_client,
    Profile* profile) {
  ProfileSyncService::InitParams init_params;

  init_params.identity_manager = IdentityManagerFactory::GetForProfile(profile);
  init_params.signin_scoped_device_id_callback =
      base::BindRepeating([]() { return std::string(); });
  init_params.start_behavior = ProfileSyncService::MANUAL_START;
  init_params.sync_client = std::move(sync_client);
  init_params.network_time_update_callback = base::DoNothing();
  bool fcm_invalidations_enabled =
      base::FeatureList::IsEnabled(invalidation::switches::kFCMInvalidations);
  if (fcm_invalidations_enabled) {
    init_params.invalidations_identity_providers.push_back(
        invalidation::ProfileInvalidationProviderFactory::GetForProfile(profile)
            ->GetIdentityProvider());
  }
  init_params.invalidations_identity_providers.push_back(
      invalidation::DeprecatedProfileInvalidationProviderFactory::GetForProfile(
          profile)
          ->GetIdentityProvider());
  init_params.url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetURLLoaderFactoryForBrowserProcess();
  init_params.debug_identifier = profile->GetDebugName();
  init_params.channel = chrome::GetChannel();

  return init_params;
}

std::unique_ptr<TestingProfile> MakeSignedInTestingProfile() {
  auto profile = std::make_unique<TestingProfile>();
  SigninManagerFactory::GetForProfile(profile.get())
      ->SetAuthenticatedAccountInfo("12345", "foo");
  return profile;
}

std::unique_ptr<KeyedService> BuildMockProfileSyncService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<browser_sync::ProfileSyncServiceMock>>(
      CreateProfileSyncServiceParamsForTest(
          Profile::FromBrowserContext(context)));
}
