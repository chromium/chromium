// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

namespace browsing_data_counter_utils {

class BrowsingDataCounterUtilsBrowserTest : public SyncTest {
 public:
  BrowsingDataCounterUtilsBrowserTest() : SyncTest(SINGLE_CLIENT) {}
  ~BrowsingDataCounterUtilsBrowserTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowsingDataCounterUtilsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(BrowsingDataCounterUtilsBrowserTest,
                       ShouldShowCookieException) {
  Profile* profile = browser()->profile();

  syncer::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetAsProfileSyncServiceForProfile(profile);

  sync_service->OverrideNetworkForTest(
      fake_server::CreateFakeServerHttpPostProviderFactory(
          GetFakeServer()->AsWeakPtr()));

  std::string username;

  if (username.empty())
    username = "user@gmail.com";

  std::unique_ptr<ProfileSyncServiceHarness> harness =
      ProfileSyncServiceHarness::Create(
          profile, username, "unused" /* password */,
          ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

  // By default, a fresh profile is not signed in, nor syncing, so no cookie
  // exception should be shown.
  EXPECT_FALSE(ShouldShowCookieException(profile));

  // Sign the profile in.
  EXPECT_TRUE(harness->SignInPrimaryAccount());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // On Chrome OS sync in turned on by default.
  EXPECT_TRUE(ShouldShowCookieException(profile));
#else
  // Sign-in alone shouldn't lead to a cookie exception.
  EXPECT_FALSE(ShouldShowCookieException(profile));
#endif

  // Enable sync.
  EXPECT_TRUE(harness->SetupSync());

  // Now that we're syncing, we should offer to retain the cookie.
  EXPECT_TRUE(ShouldShowCookieException(profile));

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // Pause sync.
  harness->SignOutPrimaryAccount();

  // There's no point in showing the cookie exception.
  EXPECT_FALSE(ShouldShowCookieException(profile));
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace browsing_data_counter_utils
