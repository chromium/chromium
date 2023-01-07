// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "components/services/storage/public/mojom/storage_usage_info.mojom.h"
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
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"

using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace {

class ExtensionBrowsingDataTest : public InProcessBrowserTest {};

class ExtensionBrowsingDataTestWithStoragePartitioning
    : public ExtensionBrowsingDataTest {
 public:
  ExtensionBrowsingDataTestWithStoragePartitioning() {
    scoped_feature_list_.InitAndEnableFeature(
        net::features::kThirdPartyStoragePartitioning);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(http://crbug.com/1266606): appcache is a noop and should be removed.
const char kRemoveEverythingArguments[] =
    R"([{"since": 1000}, {
    "appcache": true, "cache": true, "cookies": true,
    "downloads": true, "fileSystems": true, "formData": true,
    "history": true, "indexedDB": true, "localStorage": true,
    "serverBoundCertificates": true, "passwords": true,
    "pluginData": true, "serviceWorkers": true, "cacheStorage": true,
    "webSQL": true
    }])";

#if BUILDFLAG(ENABLE_DICE_SUPPORT)

// Sets the SAPISID Gaia cookie, which is monitored by the AccountReconcilor.
bool SetGaiaCookieForProfile(Profile* profile) {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "SAPISID", std::string(), "." + google_url.host(), "/", base::Time(),
      base::Time(), base::Time(), base::Time(),
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
      profile->GetDefaultStoragePartition()
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
      identity_manager, kPrimaryAccountEmail, signin::ConsentLevel::kSync);
  AccountInfo secondary_account_info =
      signin::MakeAccountAvailable(identity_manager, kSecondaryAccountEmail);

  // Sync is running.
  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile);
  sync_service->GetUserSettings()->SetSyncRequested(true);
  sync_service->GetUserSettings()->SetFirstSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  ASSERT_EQ(SyncStatusMessageType::kSynced, GetSyncStatusMessageType(profile));
  // Clear browsing data.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(nullptr,
            RunFunctionAndReturnSingleResult(
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
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, kAccountEmail, signin::ConsentLevel::kSync);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      identity_manager, account_info.account_id,
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));

  // Sync is not running.
  ASSERT_NE(SyncStatusMessageType::kSynced, GetSyncStatusMessageType(profile));
  // Clear browsing data.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(nullptr,
            RunFunctionAndReturnSingleResult(
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
  EXPECT_EQ(nullptr,
            RunFunctionAndReturnSingleResult(
                function.get(), kRemoveEverythingArguments, browser()));
  // Check that the account was removed.
  EXPECT_FALSE(
      identity_manager->HasAccountWithRefreshToken(account_info.account_id));
}
#endif

void CreateLocalStorageForKey(Profile* profile, const blink::StorageKey& key) {
  auto* local_storage_control =
      profile->GetDefaultStoragePartition()->GetLocalStorageControl();
  mojo::Remote<blink::mojom::StorageArea> area;
  local_storage_control->BindStorageArea(key,
                                         area.BindNewPipeAndPassReceiver());
  {
    bool success = false;
    base::RunLoop run_loop;
    area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, absl::nullopt,
              "source", base::BindLambdaForTesting([&](bool success_in) {
                success = success_in;
                run_loop.Quit();
              }));
    run_loop.Run();
    ASSERT_TRUE(success);
  }
}

std::vector<storage::mojom::StorageUsageInfoPtr> GetLocalStorage(
    Profile* profile) {
  auto* local_storage_control =
      profile->GetDefaultStoragePartition()->GetLocalStorageControl();
  std::vector<storage::mojom::StorageUsageInfoPtr> usage_infos;
  {
    base::RunLoop run_loop;
    local_storage_control->GetUsage(base::BindLambdaForTesting(
        [&](std::vector<storage::mojom::StorageUsageInfoPtr> usage_infos_in) {
          usage_infos.swap(usage_infos_in);
          run_loop.Quit();
        }));
    run_loop.Run();
  }
  return usage_infos;
}

bool UsageInfosHasStorageKey(
    const std::vector<storage::mojom::StorageUsageInfoPtr>& usage_infos,
    const blink::StorageKey& key) {
  auto it = base::ranges::find_if(
      usage_infos, [&key](const storage::mojom::StorageUsageInfoPtr& info) {
        return info->storage_key == key;
      });
  return it != usage_infos.end();
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, DeleteLocalStorageAll) {
  blink::StorageKey key1(url::Origin::Create(GURL("https://example.com")));
  blink::StorageKey key2(url::Origin::Create(GURL("https://other.com")));
  // Create some local storage for each of the origins.
  CreateLocalStorageForKey(browser()->profile(), key1);
  CreateLocalStorageForKey(browser()->profile(), key2);
  // Verify that the data is actually stored.
  auto usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(2U, usage_infos.size());
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key1));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key2));

  // Clear the data for everything.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_EQ(nullptr,
            RunFunctionAndReturnSingleResult(
                function.get(), kRemoveEverythingArguments, browser()));

  usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(0U, usage_infos.size());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, DeleteLocalStorageOrigin) {
  blink::StorageKey key1(url::Origin::Create(GURL("https://example.com")));
  blink::StorageKey key2(url::Origin::Create(GURL("https://other.com")));
  // Create some local storage for each of the origins.
  CreateLocalStorageForKey(browser()->profile(), key1);
  CreateLocalStorageForKey(browser()->profile(), key2);
  // Verify that the data is actually stored.
  auto usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(2U, usage_infos.size());
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key1));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key2));

  // Clear the data only for example.com.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  const char removeArgs[] =
      R"([{
    "origins": ["https://example.com"]
    }, {
    "localStorage": true
    }])";
  EXPECT_EQ(nullptr, RunFunctionAndReturnSingleResult(function.get(),
                                                      removeArgs, browser()));

  usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(1U, usage_infos.size());
  EXPECT_FALSE(UsageInfosHasStorageKey(usage_infos, key1));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key2));
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTestWithStoragePartitioning,
                       DeleteLocalStoragePartitioned) {
  ASSERT_TRUE(blink::StorageKey::IsThirdPartyStoragePartitioningEnabled());
  const auto kOrigin = url::Origin::Create(GURL("https://example.com"));
  const auto kDifferentOrigin = url::Origin::Create(GURL("https://other.com"));
  const auto kDifferentSubdomain =
      url::Origin::Create(GURL("https://maps.example.com"));
  const auto kAnotherOrigin =
      url::Origin::Create(GURL("https://something.com"));

  // First-party key for the origin being deleted.
  auto key1 = blink::StorageKey::CreateWithOptionalNonce(
      kOrigin, net::SchemefulSite(kOrigin), nullptr,
      blink::mojom::AncestorChainBit::kSameSite);
  // Third-party embedded on the origin being deleted.
  auto key2 = blink::StorageKey::CreateWithOptionalNonce(
      kDifferentOrigin, net::SchemefulSite(kOrigin), nullptr,
      blink::mojom::AncestorChainBit::kCrossSite);
  // Cross-site same origin embedded on the origin being deleted.
  auto key3 = blink::StorageKey::CreateWithOptionalNonce(
      kOrigin, net::SchemefulSite(kOrigin), nullptr,
      blink::mojom::AncestorChainBit::kCrossSite);
  // Third-party same origin embedded on a different site.
  auto key4 = blink::StorageKey::CreateWithOptionalNonce(
      kOrigin, net::SchemefulSite(kDifferentOrigin), nullptr,
      blink::mojom::AncestorChainBit::kCrossSite);
  // First-party key for an origin not being deleted.
  auto key5 = blink::StorageKey::CreateWithOptionalNonce(
      kDifferentOrigin, net::SchemefulSite(kDifferentOrigin), nullptr,
      blink::mojom::AncestorChainBit::kSameSite);
  // First-party key for a different subdomain for the origin being deleted.
  auto key6 = blink::StorageKey::CreateWithOptionalNonce(
      kDifferentSubdomain, net::SchemefulSite(kDifferentSubdomain), nullptr,
      blink::mojom::AncestorChainBit::kSameSite);
  // Third-party key with a top-level-site equal to a different subdomain for
  // the origin being deleted.
  auto key7 = blink::StorageKey::CreateWithOptionalNonce(
      kAnotherOrigin, net::SchemefulSite(kDifferentSubdomain), nullptr,
      blink::mojom::AncestorChainBit::kCrossSite);
  // Cross-site different subdomain origin embedded with itself as the top-level
  // site.
  auto key8 = blink::StorageKey::CreateWithOptionalNonce(
      kDifferentSubdomain, net::SchemefulSite(kDifferentSubdomain), nullptr,
      blink::mojom::AncestorChainBit::kCrossSite);

  std::vector<blink::StorageKey> keys = {key1, key2, key3, key4,
                                         key5, key6, key7, key8};
  // Create some local storage for each of the keys.
  for (const auto& key : keys) {
    CreateLocalStorageForKey(browser()->profile(), key);
  }

  // Verify that the data is actually stored.
  auto usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(keys.size(), usage_infos.size());
  for (const auto& key : keys) {
    EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key));
  }

  // Clear the data for example.com.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  const char removeArgs[] =
      R"([{
    "origins": ["https://example.com"]
    }, {
    "localStorage": true
    }])";
  EXPECT_EQ(nullptr, RunFunctionAndReturnSingleResult(function.get(),
                                                      removeArgs, browser()));

  usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(3U, usage_infos.size());
  EXPECT_FALSE(UsageInfosHasStorageKey(usage_infos, key1));
  EXPECT_FALSE(UsageInfosHasStorageKey(usage_infos, key2));
  EXPECT_FALSE(UsageInfosHasStorageKey(usage_infos, key3));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key4));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key5));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key6));
  EXPECT_FALSE(UsageInfosHasStorageKey(usage_infos, key7));
  EXPECT_FALSE(UsageInfosHasStorageKey(usage_infos, key8));
}
