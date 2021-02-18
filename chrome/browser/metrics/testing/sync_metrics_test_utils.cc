// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"

namespace metrics {
namespace test {

std::unique_ptr<ProfileSyncServiceHarness> InitializeProfileForSync(
    Profile* profile,
    base::WeakPtr<fake_server::FakeServer> fake_server) {
  DCHECK(profile);

  ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile)
      ->OverrideNetworkForTest(
          fake_server::CreateFakeServerHttpPostProviderFactory(
              fake_server->AsWeakPtr()));

  std::string username;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // In browser tests, the profile may already by authenticated with stub
  // account |user_manager::kStubUserEmail|.
  CoreAccountInfo info =
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountInfo(
          signin::ConsentLevel::kSync);
  username = info.email;
#endif
  if (username.empty()) {
    username = "user@gmail.com";
  }

  return ProfileSyncServiceHarness::Create(
      profile, username, "unused" /* password */,
      ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);
}

}  // namespace test
}  // namespace metrics
