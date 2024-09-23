// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
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
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/test/test_extension_dir.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"

using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

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
      net::COOKIE_PRIORITY_DEFAULT);

  base::test::TestFuture<net::CookieAccessResult> set_cookie_future;
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  cookie_manager->SetCanonicalCookie(
      *cookie, google_url, net::CookieOptions::MakeAllInclusive(),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          set_cookie_future.GetCallback(),
          net::CookieAccessResult(net::CookieInclusionStatus(
              net::CookieInclusionStatus::EXCLUDE_UNKNOWN_ERROR))));
  return set_cookie_future.Get().status.IsInclude();
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
  sync_service->SetSyncFeatureRequested();
  sync_service->GetUserSettings()->SetInitialSyncFeatureSetupComplete(
      syncer::SyncFirstSetupCompleteSource::BASIC_FLOW);

  ASSERT_EQ(SyncStatusMessageType::kSynced, GetSyncStatusMessageType(profile));
  // Clear browsing data.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(), kRemoveEverythingArguments, browser()->profile()));
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
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(), kRemoveEverythingArguments, browser()->profile()));
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
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(), kRemoveEverythingArguments, browser()->profile()));
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
    base::test::TestFuture<bool> put_future;
    area->Put({'k', 'e', 'y'}, {'v', 'a', 'l', 'u', 'e'}, std::nullopt,
              "source", put_future.GetCallback());
    ASSERT_TRUE(put_future.Get());
  }
}

std::vector<storage::mojom::StorageUsageInfoPtr> GetLocalStorage(
    Profile* profile) {
  auto* local_storage_control =
      profile->GetDefaultStoragePartition()->GetLocalStorageControl();
  base::test::TestFuture<std::vector<storage::mojom::StorageUsageInfoPtr>>
      get_usage_future;
  local_storage_control->GetUsage(get_usage_future.GetCallback());
  return get_usage_future.Take();
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
  const blink::StorageKey key1 =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey key2 =
      blink::StorageKey::CreateFromStringForTesting("https://other.com");
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
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(), kRemoveEverythingArguments, browser()->profile()));

  usage_infos = GetLocalStorage(browser()->profile());
  EXPECT_EQ(0U, usage_infos.size());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, DeleteLocalStorageIncognito) {
  const blink::StorageKey key1 =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey key2 =
      blink::StorageKey::CreateFromStringForTesting("https://other.com");
  // Create some local storage for each of the origins.
  auto* incognito_profile = browser()->profile()->GetPrimaryOTRProfile(true);
  CreateLocalStorageForKey(incognito_profile, key1);
  CreateLocalStorageForKey(incognito_profile, key2);
  // Verify that the data is actually stored.
  auto usage_infos = GetLocalStorage(incognito_profile);
  EXPECT_EQ(2U, usage_infos.size());
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key1));
  EXPECT_TRUE(UsageInfosHasStorageKey(usage_infos, key2));

  // Clear the data for everything.
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(), kRemoveEverythingArguments, incognito_profile));

  usage_infos = GetLocalStorage(incognito_profile);
  EXPECT_EQ(0U, usage_infos.size());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, DeleteLocalStorageOrigin) {
  const blink::StorageKey key1 =
      blink::StorageKey::CreateFromStringForTesting("https://example.com");
  const blink::StorageKey key2 =
      blink::StorageKey::CreateFromStringForTesting("https://other.com");
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
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(function.get(), removeArgs,
                                                browser()->profile()));

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
  auto key1 =
      blink::StorageKey::Create(kOrigin, net::SchemefulSite(kOrigin),
                                blink::mojom::AncestorChainBit::kSameSite);
  // Third-party embedded on the origin being deleted.
  auto key2 =
      blink::StorageKey::Create(kDifferentOrigin, net::SchemefulSite(kOrigin),
                                blink::mojom::AncestorChainBit::kCrossSite);
  // Cross-site same origin embedded on the origin being deleted.
  auto key3 =
      blink::StorageKey::Create(kOrigin, net::SchemefulSite(kOrigin),
                                blink::mojom::AncestorChainBit::kCrossSite);
  // Third-party same origin embedded on a different site.
  auto key4 =
      blink::StorageKey::Create(kOrigin, net::SchemefulSite(kDifferentOrigin),
                                blink::mojom::AncestorChainBit::kCrossSite);
  // First-party key for an origin not being deleted.
  auto key5 = blink::StorageKey::Create(
      kDifferentOrigin, net::SchemefulSite(kDifferentOrigin),
      blink::mojom::AncestorChainBit::kSameSite);
  // First-party key for a different subdomain for the origin being deleted.
  auto key6 = blink::StorageKey::Create(
      kDifferentSubdomain, net::SchemefulSite(kDifferentSubdomain),
      blink::mojom::AncestorChainBit::kSameSite);
  // Third-party key with a top-level-site equal to a different subdomain for
  // the origin being deleted.
  auto key7 = blink::StorageKey::Create(
      kAnotherOrigin, net::SchemefulSite(kDifferentSubdomain),
      blink::mojom::AncestorChainBit::kCrossSite);
  // Cross-site different subdomain origin embedded with itself as the top-level
  // site.
  auto key8 = blink::StorageKey::Create(
      kDifferentSubdomain, net::SchemefulSite(kDifferentSubdomain),
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
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(function.get(), removeArgs,
                                                browser()->profile()));

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

class BrowsingDataApiTest : public extensions::ExtensionApiTest {};

IN_PROC_BROWSER_TEST_F(BrowsingDataApiTest, ValidateFilters) {
  static constexpr char kManifest[] =
      R"({
           "name": "Test",
           "manifest_version": 3,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["browsingData"]
         })";

  static constexpr char kBackgroundJs[] = R"(chrome.test.runTests([
      async function originFilter() {
          await chrome.browsingData.remove(
              {'origins': ['https://example.com']},
              {'cookies': true});
          chrome.test.succeed();
      },
      async function emptyOriginsFilter() {
          const expectedError = new RegExp(
              '.* Array must have at least 1 items; found 0.');
          chrome.test.assertThrows(
              chrome.browsingData.remove,
              chrome.browsingData,
              [{'origins': []}, {'cookies': true}],
              expectedError);
          chrome.test.succeed();
      },
  ]);)";

  extensions::TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
  ASSERT_TRUE(RunExtensionTest(test_dir.UnpackedPath(), {}, {})) << message_;
}
