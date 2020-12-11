// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "url/gurl.h"

using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace {

class ExtensionBrowsingDataTest : public InProcessBrowserTest {};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

const char kRemoveEverythingArguments[] =
    R"([{"since": 1000}, {
    "appcache": true, "cache": true, "cookies": true,
    "downloads": true, "fileSystems": true, "formData": true,
    "history": true, "indexedDB": true, "localStorage": true,
    "serverBoundCertificates": true, "passwords": true,
    "pluginData": true, "serviceWorkers": true, "cacheStorage": true,
    "webSQL": true
    }])";

// Sets the SAPISID Gaia cookie, which is monitored by the AccountReconcilor.
bool SetGaiaCookieForProfile(Profile* profile) {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "SAPISID", std::string(), "." + google_url.host(), "/", base::Time(),
      base::Time(), base::Time(),
      /*secure=*/true, false, net::CookieSameSite::NO_RESTRICTION,
      net::COOKIE_PRIORITY_DEFAULT, false);

  bool success = false;
  base::RunLoop loop;
  base::OnceClosure loop_quit = loop.QuitClosure();
  base::OnceCallback<void(net::CookieAccessResult)> callback =
      base::BindLambdaForTesting(
          [&success, &loop_quit](net::CookieAccessResult r) {
            success = r.status.IsInclude();
            std::move(loop_quit).Run();
          });
  network::mojom::CookieManager* cookie_manager =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetCookieManagerForBrowserProcess();
  cookie_manager->SetCanonicalCookie(
      *cookie, google_url, net::CookieOptions::MakeAllInclusive(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          std::move(callback),
          net::CookieAccessResult(net::CookieInclusionStatus(
              net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR))));
  loop.Run();
  return success;
}
#endif

}  // namespace

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Test that Sync is not paused when browsing data is cleared.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, Syncing) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account and a secondary account.
  const char kPrimaryAccountEmail[] = "primary@email.com";
  const char kSecondaryAccountEmail[] = "secondary@email.com";

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo primary_account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kPrimaryAccountEmail);
  AccountInfo secondary_account_info =
      signin::MakeAccountAvailable(identity_manager, kSecondaryAccountEmail);

  // Sync is running.
  syncer::SyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  sync_service->GetUserSettings()->SetSyncRequested(true);
  sync_service->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  ASSERT_EQ(sync_ui_util::SYNCED, sync_ui_util::GetStatus(profile));
  // Clear browsing data.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
                      function.get(), kRemoveEverythingArguments, browser()));
  // Check that the Sync token was not revoked.
  EXPECT_TRUE(identity_manager->HasAccountWithRefreshToken(
      primary_account_info.account_id));
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshTokenInPersistentErrorState(
          primary_account_info.account_id));
  // Check that the secondary token was revoked.
  EXPECT_FALSE(identity_manager->HasAccountWithRefreshToken(
      secondary_account_info.account_id));
}

// Test that Sync is paused when browsing data is cleared if Sync was in
// authentication error.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SyncError) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account with authentication error.
  const char kAccountEmail[] = "account@email.com";
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info =
      signin::MakePrimaryAccountAvailable(identity_manager, kAccountEmail);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Sync is not running.
  ASSERT_NE(sync_ui_util::SYNCED, sync_ui_util::GetStatus(profile));
  // Clear browsing data.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
                      function.get(), kRemoveEverythingArguments, browser()));
  // Check that the account was not removed and Sync was paused.
  EXPECT_TRUE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT,
            identity_manager
                ->GetErrorStateOfRefreshTokenForAccount(account_info.account_id)
                .GetInvalidGaiaCredentialsReason());
}

// Test that the tokens are revoked when browsing data is cleared when there is
// no primary account.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, NotSyncing) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a non-Sync account.
  const char kAccountEmail[] = "account@email.com";
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info =
      signin::MakeAccountAvailable(identity_manager, kAccountEmail);
  // Clear browsing data.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
                      function.get(), kRemoveEverythingArguments, browser()));
  // Check that the account was removed.
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
}
#endif
