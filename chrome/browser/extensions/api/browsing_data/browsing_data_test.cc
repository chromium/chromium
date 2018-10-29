// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/strings/pattern.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_reconcilor_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/scoped_account_consistency.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_reconcilor.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_buildflags.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace {

enum OriginTypeMask {
  UNPROTECTED_WEB = content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
  PROTECTED_WEB = content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
  EXTENSION = ChromeBrowsingDataRemoverDelegate::ORIGIN_TYPE_EXTENSION
};

const char kRemoveEverythingArguments[] =
    "[{\"since\": 1000}, {"
    "\"appcache\": true, \"cache\": true, \"cookies\": true, "
    "\"downloads\": true, \"fileSystems\": true, \"formData\": true, "
    "\"history\": true, \"indexedDB\": true, \"localStorage\": true, "
    "\"serverBoundCertificates\": true, \"passwords\": true, "
    "\"pluginData\": true, \"serviceWorkers\": true, \"cacheStorage\": true, "
    "\"webSQL\": true"
    "}]";

class ExtensionBrowsingDataTest : public InProcessBrowserTest {
 public:
  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTime();
  }

  int GetRemovalMask() {
    return remover_->GetLastUsedRemovalMask();
  }

  int GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMask();
  }

 protected:
  void SetUp() override {
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    remover_ =
        content::BrowserContext::GetBrowsingDataRemover(browser()->profile());
  }

  int GetAsMask(const base::DictionaryValue* dict, std::string path,
                int mask_value) {
    bool result;
    EXPECT_TRUE(dict->GetBoolean(path, &result)) << "for " << path;
    return result ? mask_value : 0;
  }

  void RunBrowsingDataRemoveFunctionAndCompareRemovalMask(
      const std::string& data_types,
      int expected_mask) {
    scoped_refptr<BrowsingDataRemoveFunction> function =
        new BrowsingDataRemoveFunction();
    SCOPED_TRACE(data_types);
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(),
        std::string("[{\"since\": 1},") + data_types + "]",
        browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginTypeMask());
  }

  void RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      const std::string& key,
      int expected_mask) {
    RunBrowsingDataRemoveFunctionAndCompareRemovalMask(
        std::string("{\"") + key + "\": true}", expected_mask);
  }

  void RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      const std::string& protectedStr,
      int expected_mask) {
    scoped_refptr<BrowsingDataRemoveFunction> function =
        new BrowsingDataRemoveFunction();
    SCOPED_TRACE(protectedStr);
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(),
        "[{\"originTypes\": " + protectedStr + "}, {\"cookies\": true}]",
        browser()));
    EXPECT_EQ(expected_mask, GetOriginTypeMask());
  }

  template<class ShortcutFunction>
  void RunAndCompareRemovalMask(int expected_mask) {
    scoped_refptr<ShortcutFunction> function =
        new ShortcutFunction();
    SCOPED_TRACE(ShortcutFunction::function_name());
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(),
        std::string("[{\"since\": 1}]"),
        browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginTypeMask());
  }

  void SetSinceAndVerify(browsing_data::TimePeriod since_pref) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                      static_cast<int>(since_pref));

    scoped_refptr<BrowsingDataSettingsFunction> function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings");
    std::unique_ptr<base::Value> result_value(RunFunctionAndReturnSingleResult(
        function.get(), std::string("[]"), browser()));

    base::DictionaryValue* result;
    EXPECT_TRUE(result_value->GetAsDictionary(&result));
    base::DictionaryValue* options;
    EXPECT_TRUE(result->GetDictionary("options", &options));
    double since;
    EXPECT_TRUE(options->GetDouble("since", &since));

    double expected_since = 0;
    if (since_pref != browsing_data::TimePeriod::ALL_TIME) {
      base::Time time = CalculateBeginDeleteTime(since_pref);
      expected_since = time.ToJsTime();
    }
    // Even a synchronous function takes nonzero time, but the difference
    // between when the function was called and now should be well under a
    // second, so we'll make sure the requested start time is within 10 seconds.
    // Since the smallest selectable period is an hour, that should be
    // sufficient.
    EXPECT_LE(expected_since, since + 10.0 * 1000.0);
  }

  void SetPrefsAndVerifySettings(int data_type_flags,
                                 int expected_origin_type_mask,
                                 int expected_removal_mask) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(
        browsing_data::prefs::kLastClearBrowsingDataTab,
        static_cast<int>(browsing_data::ClearBrowsingDataTab::ADVANCED));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteCache,
        !!(data_type_flags & content::BrowsingDataRemover::DATA_TYPE_CACHE));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteCookies,
        !!(data_type_flags & content::BrowsingDataRemover::DATA_TYPE_COOKIES));
    prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory,
                      !!(data_type_flags &
                         ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteFormData,
        !!(data_type_flags &
           ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA));
    prefs->SetBoolean(browsing_data::prefs::kDeleteDownloadHistory,
                      !!(data_type_flags &
                         content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteHostedAppsData,
        !!(data_type_flags & ChromeBrowsingDataRemoverDelegate::
                                 DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY));
    prefs->SetBoolean(
        browsing_data::prefs::kDeletePasswords,
        !!(data_type_flags &
           ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS));
    prefs->SetBoolean(
        prefs::kClearPluginLSODataEnabled,
        !!(data_type_flags &
           ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA));

    VerifyRemovalMask(expected_origin_type_mask, expected_removal_mask);
  }

  void SetBasicPrefsAndVerifySettings(int data_type_flags,
                                      int expected_origin_type_mask,
                                      int expected_removal_mask) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(
        browsing_data::prefs::kLastClearBrowsingDataTab,
        static_cast<int>(browsing_data::ClearBrowsingDataTab::BASIC));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteCacheBasic,
        !!(data_type_flags & content::BrowsingDataRemover::DATA_TYPE_CACHE));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteCookiesBasic,
        !!(data_type_flags & content::BrowsingDataRemover::DATA_TYPE_COOKIES));
    prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistoryBasic,
                      !!(data_type_flags &
                         ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY));
    prefs->SetBoolean(
        prefs::kClearPluginLSODataEnabled,
        !!(data_type_flags &
           ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA));

    VerifyRemovalMask(expected_origin_type_mask, expected_removal_mask);
  }

  void VerifyRemovalMask(int expected_origin_type_mask,
                         int expected_removal_mask) {
    scoped_refptr<BrowsingDataSettingsFunction> function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings");
    std::unique_ptr<base::Value> result_value(RunFunctionAndReturnSingleResult(
        function.get(), std::string("[]"), browser()));

    base::DictionaryValue* result;
    EXPECT_TRUE(result_value->GetAsDictionary(&result));

    base::DictionaryValue* options;
    EXPECT_TRUE(result->GetDictionary("options", &options));
    base::DictionaryValue* origin_types;
    EXPECT_TRUE(options->GetDictionary("originTypes", &origin_types));
    int origin_type_mask = GetAsMask(origin_types, "unprotectedWeb",
                                    UNPROTECTED_WEB) |
                          GetAsMask(origin_types, "protectedWeb",
                                    PROTECTED_WEB) |
                          GetAsMask(origin_types, "extension", EXTENSION);
    EXPECT_EQ(expected_origin_type_mask, origin_type_mask);

    base::DictionaryValue* data_to_remove;
    EXPECT_TRUE(result->GetDictionary("dataToRemove", &data_to_remove));
    int removal_mask =
        GetAsMask(data_to_remove, "appcache",
                  content::BrowsingDataRemover::DATA_TYPE_APP_CACHE) |
        GetAsMask(data_to_remove, "cache",
                  content::BrowsingDataRemover::DATA_TYPE_CACHE) |
        GetAsMask(data_to_remove, "cookies",
                  content::BrowsingDataRemover::DATA_TYPE_COOKIES) |
        GetAsMask(data_to_remove, "downloads",
                  content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS) |
        GetAsMask(data_to_remove, "fileSystems",
                  content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS) |
        GetAsMask(data_to_remove, "formData",
                  ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA) |
        GetAsMask(data_to_remove, "history",
                  ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY) |
        GetAsMask(data_to_remove, "indexedDB",
                  content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB) |
        GetAsMask(data_to_remove, "localStorage",
                  content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE) |
        GetAsMask(data_to_remove, "pluginData",
                  ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA) |
        GetAsMask(data_to_remove, "passwords",
                  ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS) |
        GetAsMask(data_to_remove, "serviceWorkers",
                  content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS) |
        GetAsMask(data_to_remove, "cacheStorage",
                  content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE) |
        GetAsMask(data_to_remove, "webSQL",
                  content::BrowsingDataRemover::DATA_TYPE_WEB_SQL) |
        GetAsMask(data_to_remove, "serverBoundCertificates",
                  content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS);

    EXPECT_EQ(expected_removal_mask, removal_mask);
  }

  // The kAllowDeletingBrowserHistory pref must be set to false before this
  // is called.
  void CheckRemovalPermitted(const std::string& data_types, bool permitted) {
    scoped_refptr<BrowsingDataRemoveFunction> function =
        new BrowsingDataRemoveFunction();
    std::string args = "[{\"since\": 1}," + data_types + "]";

    if (permitted) {
      EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(), args, browser())) << " for " << args;
    } else {
      EXPECT_TRUE(base::MatchPattern(
          RunFunctionAndReturnError(function.get(), args, browser()),
          extension_browsing_data_api_constants::kDeleteProhibitedError))
          << " for " << args;
    }
  }

 private:
  // Cached pointer to BrowsingDataRemover for access to testing methods.
  content::BrowsingDataRemover* remover_;
};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Sets the APISID Gaia cookie, which is monitored by the AccountReconcilor.
bool SetGaiaCookieForProfile(Profile* profile) {
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  net::CanonicalCookie cookie("APISID", std::string(), "." + google_url.host(),
                              "/", base::Time(), base::Time(), base::Time(),
                              false, false, net::CookieSameSite::DEFAULT_MODE,
                              net::COOKIE_PRIORITY_DEFAULT);

  bool success = false;
  base::RunLoop loop;
  base::OnceClosure loop_quit = loop.QuitClosure();
  base::OnceCallback<void(bool)> callback =
      base::BindLambdaForTesting([&success, &loop_quit](bool s) {
        success = s;
        std::move(loop_quit).Run();
      });
  network::mojom::CookieManager* cookie_manager =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetCookieManagerForBrowserProcess();
  cookie_manager->SetCanonicalCookie(
      cookie, true, true,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback), false));
  loop.Run();
  return success;
}
#endif

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, RemovalProhibited) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  CheckRemovalPermitted("{\"appcache\": true}", true);
  CheckRemovalPermitted("{\"cache\": true}", true);
  CheckRemovalPermitted("{\"cookies\": true}", true);
  CheckRemovalPermitted("{\"downloads\": true}", false);
  CheckRemovalPermitted("{\"fileSystems\": true}", true);
  CheckRemovalPermitted("{\"formData\": true}", true);
  CheckRemovalPermitted("{\"history\": true}", false);
  CheckRemovalPermitted("{\"indexedDB\": true}", true);
  CheckRemovalPermitted("{\"localStorage\": true}", true);
  CheckRemovalPermitted("{\"serverBoundCertificates\": true}", true);
  CheckRemovalPermitted("{\"passwords\": true}", true);
  CheckRemovalPermitted("{\"serviceWorkers\": true}", true);
  CheckRemovalPermitted("{\"cacheStorage\": true}", true);
  CheckRemovalPermitted("{\"webSQL\": true}", true);

  // The entire removal is prohibited if any part is.
  CheckRemovalPermitted("{\"cache\": true, \"history\": true}", false);
  CheckRemovalPermitted("{\"cookies\": true, \"downloads\": true}", false);

  // If a prohibited type is not selected, the removal is OK.
  CheckRemovalPermitted("{\"history\": false}", true);
  CheckRemovalPermitted("{\"downloads\": false}", true);
  CheckRemovalPermitted("{\"cache\": true, \"history\": false}", true);
  CheckRemovalPermitted("{\"cookies\": true, \"downloads\": false}", true);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, RemoveBrowsingDataAll) {
  scoped_refptr<BrowsingDataRemoveFunction> function =
      new BrowsingDataRemoveFunction();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(function.get(),
                                                   kRemoveEverythingArguments,
                                                   browser()));

  EXPECT_EQ(base::Time::FromDoubleT(1.0), GetBeginTime());
  EXPECT_EQ(
      // TODO(benwells): implement clearing of site usage data via the
      // browsing data API. https://crbug.com/500801.
      // TODO(dmurph): implement clearing of durable storage permission
      // via the browsing data API. https://crbug.com/500801.
      // TODO(ramyasharma): implement clearing of external protocol data
      // via the browsing data API. https://crbug.com/692850.
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS |
          (content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE &
           ~content::BrowsingDataRemover::DATA_TYPE_BACKGROUND_FETCH &
           ~content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE) |
          content::BrowsingDataRemover::DATA_TYPE_CACHE |
          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS,
      GetRemovalMask());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// Test that Sync is not paused when browsing data is cleared.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, Syncing) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account and a secondary account.
  const char kPrimaryAccountId[] = "primary_account_id";
  const char kSecondaryAccountId[] = "secondary_account_id";
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  token_service->UpdateCredentials(kPrimaryAccountId, "token");
  ASSERT_TRUE(token_service->RefreshTokenIsAvailable(kPrimaryAccountId));
  token_service->UpdateCredentials(kSecondaryAccountId, "token");
  ASSERT_TRUE(token_service->RefreshTokenIsAvailable(kSecondaryAccountId));
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  signin_manager->SetAuthenticatedAccountInfo(kPrimaryAccountId,
                                              "user@gmail.com");
  // Sync is running.
  browser_sync::ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile);
  sync_service->SetFirstSetupComplete();
  sync_ui_util::MessageType sync_status =
      sync_ui_util::GetStatus(profile, sync_service, *signin_manager);
  ASSERT_EQ(sync_ui_util::SYNCED, sync_status);
  // Clear browsing data.
  scoped_refptr<BrowsingDataRemoveFunction> function =
      new BrowsingDataRemoveFunction();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
                      function.get(), kRemoveEverythingArguments, browser()));
  // Check that the Sync token was not revoked.
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(kPrimaryAccountId));
  EXPECT_FALSE(token_service->RefreshTokenHasError(kPrimaryAccountId));
  // Check that the secondary token was revoked.
  EXPECT_FALSE(token_service->RefreshTokenIsAvailable(kSecondaryAccountId));
}

// Test that Sync is paused when browsing data is cleared if Sync was in
// authentication error.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SyncError) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a Sync account with authentication error.
  const char kAccountId[] = "account_id";
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  token_service->UpdateCredentials(kAccountId, "token");
  ASSERT_TRUE(token_service->RefreshTokenIsAvailable(kAccountId));
  SigninManager* signin_manager = SigninManagerFactory::GetForProfile(profile);
  signin_manager->SetAuthenticatedAccountInfo(kAccountId, "user@gmail.com");
  token_service->GetDelegate()->UpdateAuthError(
      kAccountId, GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
                      GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                          CREDENTIALS_REJECTED_BY_SERVER));
  // Sync is not running.
  sync_ui_util::MessageType sync_status = sync_ui_util::GetStatus(
      profile, ProfileSyncServiceFactory::GetForProfile(profile),
      *signin_manager);
  ASSERT_NE(sync_ui_util::SYNCED, sync_status);
  // Clear browsing data.
  scoped_refptr<BrowsingDataRemoveFunction> function =
      new BrowsingDataRemoveFunction();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
                      function.get(), kRemoveEverythingArguments, browser()));
  // Check that the account was not removed and Sync was paused.
  EXPECT_TRUE(token_service->RefreshTokenIsAvailable(kAccountId));
  EXPECT_EQ(GoogleServiceAuthError::InvalidGaiaCredentialsReason::
                CREDENTIALS_REJECTED_BY_CLIENT,
            token_service->GetAuthError(kAccountId)
                .GetInvalidGaiaCredentialsReason());
}

// Test that the tokens are revoked when browsing data is cleared when there is
// no primary account.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, NotSyncing) {
  Profile* profile = browser()->profile();
  // Set a Gaia cookie.
  ASSERT_TRUE(SetGaiaCookieForProfile(profile));
  // Set a non-Sync account.
  const char kAccountId[] = "account_id";
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  token_service->UpdateCredentials(kAccountId, "token");
  ASSERT_TRUE(token_service->RefreshTokenIsAvailable(kAccountId));
  // Clear browsing data.
  scoped_refptr<BrowsingDataRemoveFunction> function =
      new BrowsingDataRemoveFunction();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
                      function.get(), kRemoveEverythingArguments, browser()));
  // Check that the account was removed.
  EXPECT_FALSE(token_service->RefreshTokenIsAvailable(kAccountId));
}
#endif

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, BrowsingDataOriginTypeMask) {
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask("{}", 0);

  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"unprotectedWeb\": true}", UNPROTECTED_WEB);
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"protectedWeb\": true}", PROTECTED_WEB);
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"extension\": true}", EXTENSION);

  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"unprotectedWeb\": true, \"protectedWeb\": true}",
      UNPROTECTED_WEB | PROTECTED_WEB);
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"unprotectedWeb\": true, \"extension\": true}",
      UNPROTECTED_WEB | EXTENSION);
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"protectedWeb\": true, \"extension\": true}",
      PROTECTED_WEB | EXTENSION);

  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      ("{\"unprotectedWeb\": true, \"protectedWeb\": true, "
       "\"extension\": true}"),
      UNPROTECTED_WEB | PROTECTED_WEB | EXTENSION);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest,
                       BrowsingDataRemovalMask) {
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "appcache", content::BrowsingDataRemover::DATA_TYPE_APP_CACHE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "cache", content::BrowsingDataRemover::DATA_TYPE_CACHE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "cookies", content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "downloads", content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "fileSystems", content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "formData", ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "history", ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "indexedDB", content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "localStorage", content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "serverBoundCertificates",
      content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "passwords", ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS);
  // We can't remove plugin data inside a test profile.
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "serviceWorkers",
      content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "cacheStorage", content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "webSQL", content::BrowsingDataRemover::DATA_TYPE_WEB_SQL);
}

// Test an arbitrary combination of data types.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest,
                       BrowsingDataRemovalMaskCombination) {
  RunBrowsingDataRemoveFunctionAndCompareRemovalMask(
      "{\"appcache\": true, \"cookies\": true, \"history\": true}",
      content::BrowsingDataRemover::DATA_TYPE_APP_CACHE |
          content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
}

// Make sure the remove() function accepts the format produced by settings().
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest,
                       BrowsingDataRemovalInputFromSettings) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetInteger(
      browsing_data::prefs::kLastClearBrowsingDataTab,
      static_cast<int>(browsing_data::ClearBrowsingDataTab::ADVANCED));
  prefs->SetBoolean(browsing_data::prefs::kDeleteCache, true);
  prefs->SetBoolean(browsing_data::prefs::kDeleteBrowsingHistory, true);
  prefs->SetBoolean(browsing_data::prefs::kDeleteDownloadHistory, true);
  prefs->SetBoolean(browsing_data::prefs::kDeleteCookies, false);
  prefs->SetBoolean(browsing_data::prefs::kDeleteFormData, false);
  prefs->SetBoolean(browsing_data::prefs::kDeleteHostedAppsData, false);
  prefs->SetBoolean(browsing_data::prefs::kDeletePasswords, false);
  prefs->SetBoolean(prefs::kClearPluginLSODataEnabled, false);
  int expected_mask = content::BrowsingDataRemover::DATA_TYPE_CACHE |
                      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY;
  std::string json;
  // Scoping for the traces.
  {
    scoped_refptr<BrowsingDataSettingsFunction> settings_function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings_json");
    std::unique_ptr<base::Value> result_value(RunFunctionAndReturnSingleResult(
        settings_function.get(), std::string("[]"), browser()));

    base::DictionaryValue* result;
    EXPECT_TRUE(result_value->GetAsDictionary(&result));
    base::DictionaryValue* data_to_remove;
    EXPECT_TRUE(result->GetDictionary("dataToRemove", &data_to_remove));

    JSONStringValueSerializer serializer(&json);
    EXPECT_TRUE(serializer.Serialize(*data_to_remove));
  }
  {
    scoped_refptr<BrowsingDataRemoveFunction> remove_function =
        new BrowsingDataRemoveFunction();
    SCOPED_TRACE("remove_json");
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        remove_function.get(),
        std::string("[{\"since\": 1},") + json + "]",
        browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginTypeMask());
  }
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, ShortcutFunctionRemovalMask) {
  RunAndCompareRemovalMask<BrowsingDataRemoveAppcacheFunction>(
      content::BrowsingDataRemover::DATA_TYPE_APP_CACHE);
  RunAndCompareRemovalMask<BrowsingDataRemoveCacheFunction>(
      content::BrowsingDataRemover::DATA_TYPE_CACHE);
  RunAndCompareRemovalMask<BrowsingDataRemoveCookiesFunction>(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
      content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS);
  RunAndCompareRemovalMask<BrowsingDataRemoveDownloadsFunction>(
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  RunAndCompareRemovalMask<BrowsingDataRemoveFileSystemsFunction>(
      content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS);
  RunAndCompareRemovalMask<BrowsingDataRemoveFormDataFunction>(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA);
  RunAndCompareRemovalMask<BrowsingDataRemoveHistoryFunction>(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  RunAndCompareRemovalMask<BrowsingDataRemoveIndexedDBFunction>(
      content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB);
  RunAndCompareRemovalMask<BrowsingDataRemoveLocalStorageFunction>(
      content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE);
  // We can't remove plugin data inside a test profile.
  RunAndCompareRemovalMask<BrowsingDataRemovePasswordsFunction>(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS);
  RunAndCompareRemovalMask<BrowsingDataRemoveServiceWorkersFunction>(
      content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS);
  RunAndCompareRemovalMask<BrowsingDataRemoveWebSQLFunction>(
      content::BrowsingDataRemover::DATA_TYPE_WEB_SQL);
}

// Test the processing of the 'delete since' preference.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionSince) {
  SetSinceAndVerify(browsing_data::TimePeriod::ALL_TIME);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_HOUR);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_DAY);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_WEEK);
  SetSinceAndVerify(browsing_data::TimePeriod::FOUR_WEEKS);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionEmpty) {
  SetPrefsAndVerifySettings(0, 0, 0);
}

// Test straightforward settings, mapped 1:1 to data types.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionSimple) {
  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_CACHE, 0,
                            content::BrowsingDataRemover::DATA_TYPE_CACHE);
  SetPrefsAndVerifySettings(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, 0,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
  SetPrefsAndVerifySettings(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA, 0,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_FORM_DATA);
  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
                            0,
                            content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  SetPrefsAndVerifySettings(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS, 0,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PASSWORDS);
  SetBasicPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                                 0,
                                 content::BrowsingDataRemover::DATA_TYPE_CACHE);
  SetBasicPrefsAndVerifySettings(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY, 0,
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY);
}

// Test cookie and app data settings.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionSiteData) {
  int supported_site_data_except_plugins =
      (content::BrowsingDataRemover::DATA_TYPE_COOKIES |
       content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS |
       content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE) &
      ~content::BrowsingDataRemover::DATA_TYPE_BACKGROUND_FETCH &
      ~content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE;
  int supported_site_data =
      supported_site_data_except_plugins |
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA;

  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                            UNPROTECTED_WEB,
                            supported_site_data_except_plugins);
  SetPrefsAndVerifySettings(
      ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY,
      PROTECTED_WEB, supported_site_data_except_plugins);
  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_COOKIES |
                                ChromeBrowsingDataRemoverDelegate::
                                    DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY,
                            PROTECTED_WEB | UNPROTECTED_WEB,
                            supported_site_data_except_plugins);
  SetPrefsAndVerifySettings(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_PLUGIN_DATA,
      UNPROTECTED_WEB, supported_site_data);
  SetBasicPrefsAndVerifySettings(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES, UNPROTECTED_WEB,
      supported_site_data_except_plugins);
}

// Test an arbitrary assortment of settings.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionAssorted) {
  int supported_site_data =
      (content::BrowsingDataRemover::DATA_TYPE_COOKIES |
       content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS |
       content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE) &
      ~content::BrowsingDataRemover::DATA_TYPE_BACKGROUND_FETCH &
      ~content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE;

  SetPrefsAndVerifySettings(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      UNPROTECTED_WEB,
      supported_site_data |
          ChromeBrowsingDataRemoverDelegate::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
}
