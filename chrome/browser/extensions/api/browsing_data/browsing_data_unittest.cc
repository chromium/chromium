// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/chrome_browsing_data_remover_constants.h"
#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/browsing_data/content/browsing_data_helper.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/history/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/test/mock_browsing_data_remover_delegate.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::api_test_utils::RunFunctionAndReturnError;
using extensions::api_test_utils::RunFunctionAndReturnSingleResult;

namespace extensions {

namespace {

enum OriginTypeMask {
  UNPROTECTED_WEB = content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
  PROTECTED_WEB = content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
  EXTENSION = chrome_browsing_data_remover::ORIGIN_TYPE_EXTENSION
};

// TODO(http://crbug.com/1266606): appcache is a noop and should be removed.
const char kRemoveEverythingArguments[] =
    R"([{"since": 1000}, {
    "appcache": true, "cache": true, "cookies": true, "downloads": true,
    "fileSystems": true, "formData": true, "history": true, "indexedDB": true,
    "localStorage": true, "serverBoundCertificates": true, "passwords": true,
    "pluginData": true, "serviceWorkers": true, "cacheStorage": true, "webSQL":
    true
    }])";

class BrowsingDataApiTest : public ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));

    remover_ = profile()->GetBrowsingDataRemover();
    remover_->SetEmbedderDelegate(&delegate_);
  }

  void TearDown() override {
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestBase::TearDown();
  }
  const base::Time& GetBeginTime() {
    return remover_->GetLastUsedBeginTimeForTesting();
  }

  uint64_t GetRemovalMask() {
    return remover_->GetLastUsedRemovalMaskForTesting();
  }

  uint64_t GetOriginTypeMask() {
    return remover_->GetLastUsedOriginTypeMaskForTesting();
  }

  uint64_t GetAsMask(const base::Value::Dict* dict,
                     std::string path,
                     uint64_t mask_value) {
    std::optional<bool> value = dict->FindBool(path);
    DCHECK(value.has_value());
    return *value ? mask_value : 0;
  }

  void RunBrowsingDataRemoveFunctionAndCompareRemovalMask(
      const std::string& data_types,
      uint64_t expected_mask) {
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    SCOPED_TRACE(data_types);
    EXPECT_FALSE(RunFunctionAndReturnSingleResult(
        function.get(), std::string("[{\"since\": 1},") + data_types + "]",
        browser()->profile()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginTypeMask());
  }

  void RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      const std::string& key,
      uint64_t expected_mask) {
    RunBrowsingDataRemoveFunctionAndCompareRemovalMask(
        std::string("{\"") + key + "\": true}", expected_mask);
  }

  void RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      const std::string& protectedStr,
      uint64_t expected_mask) {
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    SCOPED_TRACE(protectedStr);
    EXPECT_FALSE(RunFunctionAndReturnSingleResult(
        function.get(),
        "[{\"originTypes\": " + protectedStr + "}, {\"cookies\": true}]",
        browser()->profile()));
    EXPECT_EQ(expected_mask, GetOriginTypeMask());
  }

  template <class ShortcutFunction>
  void RunAndCompareRemovalMask(uint64_t expected_mask) {
    scoped_refptr<ShortcutFunction> function = new ShortcutFunction();
    SCOPED_TRACE(ShortcutFunction::static_function_name());
    EXPECT_FALSE(RunFunctionAndReturnSingleResult(
        function.get(), std::string("[{\"since\": 1}]"), browser()->profile()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginTypeMask());
  }

  void SetSinceAndVerify(browsing_data::TimePeriod since_pref) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    browsing_data::ClearBrowsingDataTab tab =
        static_cast<browsing_data::ClearBrowsingDataTab>(
            prefs->GetInteger(browsing_data::prefs::kLastClearBrowsingDataTab));
    auto* time_period_pref = browsing_data::GetTimePeriodPreferenceName(tab);
    prefs->SetInteger(time_period_pref, static_cast<int>(since_pref));

    scoped_refptr<BrowsingDataSettingsFunction> function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings");
    std::optional<base::Value> result = RunFunctionAndReturnSingleResult(
        function.get(), std::string("[]"), browser()->profile());
    EXPECT_TRUE(result->is_dict());
    ASSERT_TRUE(result->GetDict().FindDoubleByDottedPath("options.since"));
    double since = *result->GetDict().FindDoubleByDottedPath("options.since");

    double expected_since = 0;
    if (since_pref != browsing_data::TimePeriod::ALL_TIME) {
      base::Time time = CalculateBeginDeleteTime(since_pref);
      expected_since = time.InMillisecondsFSinceUnixEpoch();
    }
    // Even a synchronous function takes nonzero time, but the difference
    // between when the function was called and now should be well under a
    // second, so we'll make sure the requested start time is within 10 seconds.
    // Since the smallest selectable period is an hour, that should be
    // sufficient.
    EXPECT_LE(expected_since, since + 10.0 * 1000.0);
  }

  void SetPrefsAndVerifySettings(uint64_t data_type_flags,
                                 uint64_t expected_origin_type_mask,
                                 uint64_t expected_removal_mask) {
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
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteBrowsingHistory,
        !!(data_type_flags & chrome_browsing_data_remover::DATA_TYPE_HISTORY));
    prefs->SetBoolean(browsing_data::prefs::kDeleteFormData,
                      !!(data_type_flags &
                         chrome_browsing_data_remover::DATA_TYPE_FORM_DATA));
    prefs->SetBoolean(browsing_data::prefs::kDeleteDownloadHistory,
                      !!(data_type_flags &
                         content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS));
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteHostedAppsData,
        !!(data_type_flags &
           chrome_browsing_data_remover::DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY));
    prefs->SetBoolean(browsing_data::prefs::kDeletePasswords,
                      !!(data_type_flags &
                         chrome_browsing_data_remover::DATA_TYPE_PASSWORDS));

    VerifyRemovalMask(expected_origin_type_mask, expected_removal_mask);
  }

  void SetBasicPrefsAndVerifySettings(uint64_t data_type_flags,
                                      uint64_t expected_origin_type_mask,
                                      uint64_t expected_removal_mask) {
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
    prefs->SetBoolean(
        browsing_data::prefs::kDeleteBrowsingHistoryBasic,
        !!(data_type_flags & chrome_browsing_data_remover::DATA_TYPE_HISTORY));

    VerifyRemovalMask(expected_origin_type_mask, expected_removal_mask);
  }

  void VerifyRemovalMask(uint64_t expected_origin_type_mask,
                         uint64_t expected_removal_mask) {
    scoped_refptr<BrowsingDataSettingsFunction> function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings");
    std::optional<base::Value> result = RunFunctionAndReturnSingleResult(
        function.get(), std::string("[]"), browser()->profile());

    ASSERT_TRUE(result->is_dict());
    const base::Value::Dict& result_dict = result->GetDict();
    const base::Value::Dict* origin_types =
        result_dict.FindDictByDottedPath("options.originTypes");

    ASSERT_TRUE(origin_types);
    uint64_t origin_type_mask =
        GetAsMask(origin_types, "unprotectedWeb", UNPROTECTED_WEB) |
        GetAsMask(origin_types, "protectedWeb", PROTECTED_WEB) |
        GetAsMask(origin_types, "extension", EXTENSION);
    EXPECT_EQ(expected_origin_type_mask, origin_type_mask);

    const base::Value::Dict* data_to_remove =
        result_dict.FindDict("dataToRemove");
    ASSERT_TRUE(data_to_remove);
    uint64_t removal_mask =
        GetAsMask(data_to_remove, "cache",
                  content::BrowsingDataRemover::DATA_TYPE_CACHE) |
        GetAsMask(data_to_remove, "cacheStorage",
                  content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE) |
        GetAsMask(data_to_remove, "cookies",
                  content::BrowsingDataRemover::DATA_TYPE_COOKIES) |
        GetAsMask(data_to_remove, "downloads",
                  content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS) |
        GetAsMask(data_to_remove, "fileSystems",
                  content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS) |
        GetAsMask(data_to_remove, "formData",
                  chrome_browsing_data_remover::DATA_TYPE_FORM_DATA) |
        GetAsMask(data_to_remove, "history",
                  chrome_browsing_data_remover::DATA_TYPE_HISTORY) |
        GetAsMask(data_to_remove, "indexedDB",
                  content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB) |
        GetAsMask(data_to_remove, "localStorage",
                  content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE) |
        // PluginData is not supported anymore.
        GetAsMask(data_to_remove, "pluginData", 0) |
        GetAsMask(data_to_remove, "passwords",
                  chrome_browsing_data_remover::DATA_TYPE_PASSWORDS) |
        GetAsMask(data_to_remove, "serviceWorkers",
                  content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS) |
        GetAsMask(data_to_remove, "webSQL",
                  content::BrowsingDataRemover::DATA_TYPE_WEB_SQL);

    EXPECT_EQ(expected_removal_mask, removal_mask);
  }

  // The kAllowDeletingBrowserHistory pref must be set to false before this
  // is called.
  void CheckRemovalPermitted(const std::string& data_types, bool permitted) {
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    std::string args = "[{\"since\": 1}," + data_types + "]";

    if (permitted) {
      EXPECT_FALSE(RunFunctionAndReturnSingleResult(function.get(), args,
                                                    browser()->profile()))
          << " for " << args;
    } else {
      EXPECT_EQ(
          RunFunctionAndReturnError(function.get(), args, browser()->profile()),
          extension_browsing_data_api_constants::kDeleteProhibitedError)
          << " for " << args;
    }
  }

  void CheckInvalidRemovalArgs(const std::string& args,
                               const std::string& expected_error) {
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    EXPECT_EQ(
        RunFunctionAndReturnError(function.get(), args, browser()->profile()),
        expected_error)
        << " for " << args;
  }

  void VerifyFilterBuilder(const std::string& options,
                           content::BrowsingDataFilterBuilder* filter_builder) {
    delegate()->ExpectCall(
        base::Time::UnixEpoch(), base::Time::Max(),
        content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE, UNPROTECTED_WEB,
        filter_builder);
    auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    EXPECT_FALSE(RunFunctionAndReturnSingleResult(
        function.get(), "[" + options + ", {\"localStorage\": true}]",
        browser()->profile()))
        << options;
    delegate()->VerifyAndClearExpectations();
  }

  Browser* browser() { return browser_.get(); }

  content::MockBrowsingDataRemoverDelegate* delegate() { return &delegate_; }

 private:
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
  raw_ptr<content::BrowsingDataRemover> remover_;
  content::MockBrowsingDataRemoverDelegate delegate_;
};

}  // namespace

TEST_F(BrowsingDataApiTest, RemovalProhibited) {
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAllowDeletingBrowserHistory, false);

  CheckRemovalPermitted("{\"cache\": true}", true);
  CheckRemovalPermitted("{\"cacheStorage\": true}", true);
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

TEST_F(BrowsingDataApiTest, RemoveBrowsingDataAll) {
  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(), kRemoveEverythingArguments, browser()->profile()));

  EXPECT_EQ(base::Time::FromSecondsSinceUnixEpoch(1.0), GetBeginTime());
  EXPECT_EQ(
      // TODO(benwells): implement clearing of site usage data via the
      // browsing data API. https://crbug.com/500801.
      // TODO(dmurph): implement clearing of durable storage permission
      // via the browsing data API. https://crbug.com/500801.
      // TODO(ramyasharma): implement clearing of external protocol data
      // via the browsing data API. https://crbug.com/692850.
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          (content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE &
           ~content::BrowsingDataRemover::DATA_TYPE_BACKGROUND_FETCH &
           ~content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE) |
          content::BrowsingDataRemover::DATA_TYPE_CACHE |
          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
          chrome_browsing_data_remover::DATA_TYPE_FORM_DATA |
          chrome_browsing_data_remover::DATA_TYPE_HISTORY |
          chrome_browsing_data_remover::DATA_TYPE_PASSWORDS,
      GetRemovalMask());
}

TEST_F(BrowsingDataApiTest, BrowsingDataOriginTypeMask) {
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask("{}", 0);

  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"unprotectedWeb\": true}", UNPROTECTED_WEB);
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask(
      "{\"protectedWeb\": true}", PROTECTED_WEB);
  RunBrowsingDataRemoveFunctionAndCompareOriginTypeMask("{\"extension\": true}",
                                                        EXTENSION);

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

TEST_F(BrowsingDataApiTest, BrowsingDataRemovalMask) {
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "cache", content::BrowsingDataRemover::DATA_TYPE_CACHE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "cacheStorage", content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "cookies", content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "downloads", content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "fileSystems", content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "formData", chrome_browsing_data_remover::DATA_TYPE_FORM_DATA);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "history", chrome_browsing_data_remover::DATA_TYPE_HISTORY);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "indexedDB", content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "localStorage", content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "passwords", chrome_browsing_data_remover::DATA_TYPE_PASSWORDS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "serviceWorkers",
      content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS);
  RunBrowsingDataRemoveWithKeyAndCompareRemovalMask(
      "webSQL", content::BrowsingDataRemover::DATA_TYPE_WEB_SQL);
}

// Test an arbitrary combination of data types.
TEST_F(BrowsingDataApiTest, BrowsingDataRemovalMaskCombination) {
  RunBrowsingDataRemoveFunctionAndCompareRemovalMask(
      "{\"cookies\": true, \"history\": true}",
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          chrome_browsing_data_remover::DATA_TYPE_HISTORY);
}

// Make sure the remove() function accepts the format produced by settings().
TEST_F(BrowsingDataApiTest, BrowsingDataRemovalInputFromSettings) {
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
  uint64_t expected_mask = content::BrowsingDataRemover::DATA_TYPE_CACHE |
                           content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                           chrome_browsing_data_remover::DATA_TYPE_HISTORY;
  std::string json;
  // Scoping for the traces.
  {
    scoped_refptr<BrowsingDataSettingsFunction> settings_function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings_json");
    std::optional<base::Value> result = RunFunctionAndReturnSingleResult(
        settings_function.get(), std::string("[]"), browser()->profile());

    EXPECT_TRUE(result->is_dict());
    base::Value::Dict* data_to_remove =
        result->GetDict().FindDict("dataToRemove");
    EXPECT_TRUE(data_to_remove);

    JSONStringValueSerializer serializer(&json);
    EXPECT_TRUE(serializer.Serialize(*data_to_remove));
  }
  {
    auto remove_function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
    SCOPED_TRACE("remove_json");
    EXPECT_FALSE(RunFunctionAndReturnSingleResult(
        remove_function.get(), std::string("[{\"since\": 1},") + json + "]",
        browser()->profile()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginTypeMask());
  }
}

TEST_F(BrowsingDataApiTest, ShortcutFunctionRemovalMask) {
  RunAndCompareRemovalMask<BrowsingDataRemoveCacheFunction>(
      content::BrowsingDataRemover::DATA_TYPE_CACHE);
  RunAndCompareRemovalMask<BrowsingDataRemoveCacheStorageFunction>(
      content::BrowsingDataRemover::DATA_TYPE_CACHE_STORAGE);
  RunAndCompareRemovalMask<BrowsingDataRemoveCookiesFunction>(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES);
  RunAndCompareRemovalMask<BrowsingDataRemoveDownloadsFunction>(
      content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  RunAndCompareRemovalMask<BrowsingDataRemoveFileSystemsFunction>(
      content::BrowsingDataRemover::DATA_TYPE_FILE_SYSTEMS);
  RunAndCompareRemovalMask<BrowsingDataRemoveFormDataFunction>(
      chrome_browsing_data_remover::DATA_TYPE_FORM_DATA);
  RunAndCompareRemovalMask<BrowsingDataRemoveHistoryFunction>(
      chrome_browsing_data_remover::DATA_TYPE_HISTORY);
  RunAndCompareRemovalMask<BrowsingDataRemoveIndexedDBFunction>(
      content::BrowsingDataRemover::DATA_TYPE_INDEXED_DB);
  RunAndCompareRemovalMask<BrowsingDataRemoveLocalStorageFunction>(
      content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE);
  RunAndCompareRemovalMask<BrowsingDataRemovePasswordsFunction>(
      chrome_browsing_data_remover::DATA_TYPE_PASSWORDS);
  RunAndCompareRemovalMask<BrowsingDataRemoveServiceWorkersFunction>(
      content::BrowsingDataRemover::DATA_TYPE_SERVICE_WORKERS);
  RunAndCompareRemovalMask<BrowsingDataRemoveWebSQLFunction>(
      content::BrowsingDataRemover::DATA_TYPE_WEB_SQL);
}

// Test the processing of the 'delete since' preference.
TEST_F(BrowsingDataApiTest, SettingsFunctionSince) {
  SetSinceAndVerify(browsing_data::TimePeriod::ALL_TIME);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_15_MINUTES);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_HOUR);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_DAY);
  SetSinceAndVerify(browsing_data::TimePeriod::LAST_WEEK);
  SetSinceAndVerify(browsing_data::TimePeriod::FOUR_WEEKS);
}

TEST_F(BrowsingDataApiTest, SettingsFunctionEmpty) {
  SetPrefsAndVerifySettings(0, 0, 0);
}

// Test straightforward settings, mapped 1:1 to data types.
TEST_F(BrowsingDataApiTest, SettingsFunctionSimple) {
  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_CACHE, 0,
                            content::BrowsingDataRemover::DATA_TYPE_CACHE);
  SetPrefsAndVerifySettings(chrome_browsing_data_remover::DATA_TYPE_HISTORY, 0,
                            chrome_browsing_data_remover::DATA_TYPE_HISTORY);
  SetPrefsAndVerifySettings(chrome_browsing_data_remover::DATA_TYPE_FORM_DATA,
                            0,
                            chrome_browsing_data_remover::DATA_TYPE_FORM_DATA);
  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
                            0,
                            content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  SetPrefsAndVerifySettings(chrome_browsing_data_remover::DATA_TYPE_PASSWORDS,
                            0,
                            chrome_browsing_data_remover::DATA_TYPE_PASSWORDS);
  SetBasicPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_CACHE,
                                 0,
                                 content::BrowsingDataRemover::DATA_TYPE_CACHE);
  SetBasicPrefsAndVerifySettings(
      chrome_browsing_data_remover::DATA_TYPE_HISTORY, 0,
      chrome_browsing_data_remover::DATA_TYPE_HISTORY);
}

// Test cookie and app data settings.
TEST_F(BrowsingDataApiTest, SettingsFunctionSiteData) {
  uint64_t supported_site_data =
      (content::BrowsingDataRemover::DATA_TYPE_COOKIES |
       content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE) &
      ~content::BrowsingDataRemover::DATA_TYPE_BACKGROUND_FETCH &
      ~content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE;
  SetPrefsAndVerifySettings(content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                            UNPROTECTED_WEB, supported_site_data);
  SetPrefsAndVerifySettings(
      chrome_browsing_data_remover::DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY,
      PROTECTED_WEB, supported_site_data);
  SetPrefsAndVerifySettings(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          chrome_browsing_data_remover::DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY,
      PROTECTED_WEB | UNPROTECTED_WEB, supported_site_data);
  SetBasicPrefsAndVerifySettings(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES, UNPROTECTED_WEB,
      supported_site_data);
}

// Test an arbitrary assortment of settings.
TEST_F(BrowsingDataApiTest, SettingsFunctionAssorted) {
  uint64_t supported_site_data =
      (content::BrowsingDataRemover::DATA_TYPE_COOKIES |
       content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE) &
      ~content::BrowsingDataRemover::DATA_TYPE_BACKGROUND_FETCH &
      ~content::BrowsingDataRemover::DATA_TYPE_EMBEDDER_DOM_STORAGE;

  SetPrefsAndVerifySettings(
      content::BrowsingDataRemover::DATA_TYPE_COOKIES |
          chrome_browsing_data_remover::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS,
      UNPROTECTED_WEB,
      supported_site_data | chrome_browsing_data_remover::DATA_TYPE_HISTORY |
          content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
}

TEST_F(BrowsingDataApiTest, RemoveWithoutFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kPreserve);
  ASSERT_TRUE(filter_builder->MatchesAllOriginsAndDomains());

  VerifyFilterBuilder("{}", filter_builder.get());
}

TEST_F(BrowsingDataApiTest, RemoveWithDeleteListFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder->AddOrigin(url::Origin::Create(GURL("http://example.com")));

  VerifyFilterBuilder(R"({"origins": ["http://example.com"]})",
                      filter_builder.get());
}

TEST_F(BrowsingDataApiTest, RemoveWithPreserveListFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kPreserve);
  filter_builder->AddOrigin(url::Origin::Create(GURL("http://example.com")));

  VerifyFilterBuilder(R"({"excludeOrigins": ["http://example.com"]})",
                      filter_builder.get());
}

TEST_F(BrowsingDataApiTest, RemoveWithSpecialUrlFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kPreserve);
  filter_builder->AddOrigin(url::Origin::Create(GURL("file:///")));
  filter_builder->AddOrigin(url::Origin::Create(GURL("http://example.com")));

  VerifyFilterBuilder(
      R"({"excludeOrigins": ["file:///tmp/foo.html/",
          "filesystem:http://example.com/foo.txt"]})",
      filter_builder.get());
}

TEST_F(BrowsingDataApiTest, RemoveCookiesWithFilter) {
  auto filter_builder = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kPreserve);
  filter_builder->AddRegisterableDomain("example.com");
  delegate()->ExpectCall(base::Time::UnixEpoch(), base::Time::Max(),
                         content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                         UNPROTECTED_WEB, filter_builder.get());

  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(),
      R"([{"excludeOrigins": ["http://example.com"]},
                    {"cookies": true}])",
      browser()->profile()));
  delegate()->VerifyAndClearExpectations();
}

// Expect two separate BrowsingDataRemover calls if cookies and localstorage
// are passed with a filter.
TEST_F(BrowsingDataApiTest, RemoveCookiesAndStorageWithFilter) {
  auto filter_builder1 = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder1->AddRegisterableDomain("example.com");
  delegate()->ExpectCall(base::Time::UnixEpoch(), base::Time::Max(),
                         content::BrowsingDataRemover::DATA_TYPE_COOKIES,
                         UNPROTECTED_WEB, filter_builder1.get());

  auto filter_builder2 = content::BrowsingDataFilterBuilder::Create(
      content::BrowsingDataFilterBuilder::Mode::kDelete);
  filter_builder2->AddOrigin(
      url::Origin::Create(GURL("http://www.example.com")));
  delegate()->ExpectCall(base::Time::UnixEpoch(), base::Time::Max(),
                         content::BrowsingDataRemover::DATA_TYPE_LOCAL_STORAGE,
                         UNPROTECTED_WEB, filter_builder2.get());

  auto function = base::MakeRefCounted<BrowsingDataRemoveFunction>();
  EXPECT_FALSE(RunFunctionAndReturnSingleResult(
      function.get(),
      R"([{"origins": ["http://www.example.com"]},
                    {"cookies": true, "localStorage": true}])",
      browser()->profile()));
  delegate()->VerifyAndClearExpectations();
}

TEST_F(BrowsingDataApiTest, RemoveWithFilterAndInvalidParameters) {
  CheckInvalidRemovalArgs(
      R"([{"origins": ["http://example.com"]}, {"passwords": true}])",
      extension_browsing_data_api_constants::kNonFilterableError);

  CheckInvalidRemovalArgs(
      R"([{"origins": ["http://example.com"],
          "excludeOrigins": ["https://example.com"]},
          {"localStorage": true}])",
      extension_browsing_data_api_constants::kIncompatibleFilterError);

  CheckInvalidRemovalArgs(
      R"([{"origins": ["foo"]}, {"localStorage": true}])",
      base::StringPrintf(
          extension_browsing_data_api_constants::kInvalidOriginError, "foo"));

  CheckInvalidRemovalArgs(
      R"([{"excludeOrigins": ["foo"]}, {"localStorage": true}])",
      base::StringPrintf(
          extension_browsing_data_api_constants::kInvalidOriginError, "foo"));
}
}  // namespace extensions
