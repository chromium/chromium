// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/counters/browsing_data_counter_utils.h"

#include "build/buildflag.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/sync/test/fake_server/fake_server_network_resources.h"
#include "components/unified_consent/feature.h"
#include "components/unified_consent/scoped_unified_consent.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || defined(OS_CHROMEOS)
#include "chrome/browser/signin/scoped_account_consistency.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager_base.h"
#endif

namespace browsing_data_counter_utils {

class BrowsingDataCounterUtilsBrowserTest
    : public SyncTest,
      public testing::WithParamInterface<bool> {
 public:
  BrowsingDataCounterUtilsBrowserTest()
      : SyncTest(SINGLE_CLIENT),
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
        scoped_dice_(GetParam()
                         ? std::make_unique<ScopedAccountConsistencyDice>()
                         : nullptr),
#elif defined(OS_CHROMEOS)
        scoped_mirror_(std::make_unique<ScopedAccountConsistencyMirror>()),
#endif
        scoped_unified_consent_(
            GetParam()
                ? unified_consent::UnifiedConsentFeatureState::kEnabledNoBump
                : unified_consent::UnifiedConsentFeatureState::kDisabled) {
  }
  ~BrowsingDataCounterUtilsBrowserTest() override = default;

 private:
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // ScopedAccountConsistencyDice is required for unified consent to be enabled.
  const std::unique_ptr<ScopedAccountConsistencyDice> scoped_dice_;
#elif defined(OS_CHROMEOS)
  // Need to manually turn on mirror for now.
  const std::unique_ptr<ScopedAccountConsistencyMirror> scoped_mirror_;
#endif
  const unified_consent::ScopedUnifiedConsent scoped_unified_consent_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataCounterUtilsBrowserTest);
};

// Instantiate test for unified consent disabled & enabled.
INSTANTIATE_TEST_CASE_P(,
                        BrowsingDataCounterUtilsBrowserTest,
                        ::testing::Bool());

IN_PROC_BROWSER_TEST_P(BrowsingDataCounterUtilsBrowserTest,
                       ShouldShowCookieException) {
  Profile* profile = browser()->profile();

  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

  sync_service->OverrideNetworkResourcesForTest(
      std::make_unique<fake_server::FakeServerNetworkResources>(
          GetFakeServer()->AsWeakPtr()));

  std::string username;
#if defined(OS_CHROMEOS)
  // In browser tests, the profile may already by authenticated with stub
  // account |user_manager::kStubUserEmail|.
  AccountInfo info = SigninManagerFactory::GetForProfile(profile)
                         ->GetAuthenticatedAccountInfo();
  username = info.email;
#endif
  if (username.empty())
    username = "user@gmail.com";

  std::unique_ptr<ProfileSyncServiceHarness> harness =
      ProfileSyncServiceHarness::Create(
          profile, username, "unused" /* password */,
          ProfileSyncServiceHarness::SigninType::FAKE_SIGNIN);

#if defined(OS_CHROMEOS)
  // On Chrome OS, the profile is always authenticated.
  EXPECT_TRUE(ShouldShowCookieException(profile));
#else
  // By default, a fresh profile is not signed in, nor syncing, so no cookie
  // exception should be shown.
  EXPECT_FALSE(ShouldShowCookieException(profile));

  // Sign the profile in.
  EXPECT_TRUE(harness->SignInPrimaryAccount());

  // Sign-in alone shouldn't lead to a cookie exception.
  EXPECT_FALSE(ShouldShowCookieException(profile));
#endif

  // Enable sync.
  EXPECT_TRUE(harness->SetupSync());

  // Now that we're syncing, we should offer to retain the cookie.
  EXPECT_TRUE(ShouldShowCookieException(profile));

#if !defined(OS_CHROMEOS)
  // Pause sync.
  harness->SignOutPrimaryAccount();

  // There's no point in showing the cookie exception.
  EXPECT_FALSE(ShouldShowCookieException(profile));
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace browsing_data_counter_utils
