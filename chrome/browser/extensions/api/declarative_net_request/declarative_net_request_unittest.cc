// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/api/declarative_net_request.h"

#include <stddef.h>

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/declarative_net_request_api.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/prefs_helper.h"
#include "extensions/browser/api/declarative_net_request/rule_counts.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/url_pattern.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";

constexpr char kSmallRegexFilter[] = "http://(yahoo|google)\\.com";
constexpr char kLargeRegexFilter[] = ".{512}x";

constexpr char kId1[] = "1.json";
constexpr char kId2[] = "2.json";
constexpr char kId3[] = "3.json";
constexpr char kId4[] = "4.json";
constexpr char kDefaultRulesetID[] = "id";

namespace dnr_api = extensions::api::declarative_net_request;

using ::testing::ElementsAreArray;
using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

template <class T>
base::Value::List VectorToList(const std::vector<T>& values) {
  base::Value::List lv;
  for (const auto& value : values) {
    lv.Append(value);
  }
  return lv;
}

std::string GetErrorWithFilename(
    const std::string& error,
    const std::string& filename = kJSONRulesFilename) {
  return base::StringPrintf("%s: %s", filename.c_str(), error.c_str());
}

InstallWarning GetLargeRegexWarning(
    int rule_id,
    const std::string& filename = kJSONRulesFilename) {
  return InstallWarning(ErrorUtils::FormatErrorMessage(
                            GetErrorWithFilename(kErrorRegexTooLarge, filename),
                            base::NumberToString(rule_id), kRegexFilterKey),
                        dnr_api::ManifestKeys::kDeclarativeNetRequest,
                        dnr_api::DNRInfo::kRuleResources);
}

// Returns the vector of install warnings, filtering out the one associated with
// a deprecated manifest version.
// TODO(crbug.com/40804030): Remove this method when the associated tests
// are updated to MV3.
std::vector<InstallWarning> GetFilteredInstallWarnings(
    const Extension& extension) {
  std::vector<InstallWarning> filtered_warnings;
  // InstallWarning isn't copyable (but is movable), so we have to do a bit of
  // extra legwork to get a vector here.
  for (const auto& warning : extension.install_warnings()) {
    if (warning.message == manifest_errors::kManifestV2IsDeprecatedWarning) {
      continue;
    }
    filtered_warnings.emplace_back(warning.message, warning.key,
                                   warning.specific);
  }
  return filtered_warnings;
}

// Base test fixture to test indexing of rulesets.
class DeclarativeNetRequestUnittest : public DNRTestBase {
 public:
  DeclarativeNetRequestUnittest() = default;

  // DNRTestBase override.
  void SetUp() override {
    DNRTestBase::SetUp();

    RulesMonitorService::GetFactoryInstance()->SetTestingFactory(
        browser_context(),
        base::BindRepeating([](content::BrowserContext* context) {
          return static_cast<std::unique_ptr<KeyedService>>(
              RulesMonitorService::CreateInstanceForTesting(context));
        }));
    ASSERT_TRUE(RulesMonitorService::Get(browser_context()));

    extension_prefs_ = ExtensionPrefs::Get(browser_context());
    loader_ = CreateExtensionLoader();
    extension_dir_ =
        temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension"));

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir_));

    // Sanity check that the extension can index and enable up to
    // |rule_limit_override_| + |global_limit_override_| rules.
    ASSERT_EQ(300, GetMaximumRulesPerRuleset());
  }

 protected:
  RulesetManager* manager() {
    return RulesMonitorService::Get(browser_context())->ruleset_manager();
  }

  // Loads the extension and verifies the indexed ruleset location and histogram
  // counts.
  void LoadAndExpectSuccess(size_t expected_rules_count,
                            size_t expected_enabled_rules_count,
                            bool expect_rulesets_indexed) {
    base::HistogramTester tester;
    WriteExtensionData();

    loader_->set_should_fail(false);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    ASSERT_TRUE(extension_.get());

    auto ruleset_filter = FileBackedRulesetSource::RulesetFilter::kIncludeAll;
    if (GetParam() == ExtensionLoadType::PACKED) {
      ruleset_filter =
          FileBackedRulesetSource::RulesetFilter::kIncludeManifestEnabled;
    }
    EXPECT_TRUE(AreAllIndexedStaticRulesetsValid(*extension_, browser_context(),
                                                 ruleset_filter));

    // Ensure no load errors were reported.
    EXPECT_TRUE(error_reporter()->GetErrors()->empty());

    // The histograms below are not logged for unpacked extensions.
    if (GetParam() == ExtensionLoadType::PACKED) {
      int expected_samples = expect_rulesets_indexed ? 1 : 0;

      tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                              expected_samples);
      tester.ExpectUniqueSample(kManifestEnabledRulesCountHistogram,
                                expected_enabled_rules_count, expected_samples);
    }
  }

  void LoadAndExpectError(const std::string& expected_error,
                          const std::string& filename) {
    // The error should be prepended with the JSON filename.
    std::string error_with_filename =
        GetErrorWithFilename(expected_error, filename);

    base::HistogramTester tester;
    WriteExtensionData();

    loader_->set_should_fail(true);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    EXPECT_FALSE(extension_.get());

    // Verify the error. Only verify if the |expected_error| is a substring of
    // the actual error, since some string may be prepended/appended while
    // creating the actual error.
    const std::vector<std::u16string>* errors = error_reporter()->GetErrors();
    ASSERT_EQ(1u, errors->size());
    EXPECT_NE(std::u16string::npos,
              errors->at(0).find(base::UTF8ToUTF16(error_with_filename)))
        << "expected: " << error_with_filename << " actual: " << errors->at(0);

    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kManifestEnabledRulesCountHistogram, 0u);
  }

  void LoadAndExpectParseFailure(ParseResult parse_result,
                                 int rule_id,
                                 const std::string& filename) {
    std::string expected_error = GetParseError(parse_result, rule_id);

    if (GetParam() == ExtensionLoadType::UNPACKED) {
      // All static rulesets are indexed (and therefore all static rules are
      // parsed) at installation time for unpacked extensions, with invalid
      // rules resulting in a hard installation error where possible.
      LoadAndExpectError(expected_error, filename);
      return;
    }

    // Static ruleset indexing for packed extensions is deferred until the
    // ruleset is enabled. Invalid static rules are ignored with a warning
    // raised.

    base::HistogramTester tester;
    WriteExtensionData();

    loader_->set_should_fail(false);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);

    // Neither warnings nor errors are raised for problematic static rules for
    // packed extensions.
    EXPECT_TRUE(extension_.get());
    const std::vector<std::u16string>* errors = error_reporter()->GetErrors();
    ASSERT_TRUE(errors->empty());
  }

  enum class RulesetScope { kDynamic, kSession };

  // Runs the updateDynamicRules/updateSessionRules extension function based on
  // |scope| and verifies the success/failure based on |expected_error|.
  void RunUpdateRulesFunction(const Extension& extension,
                              const std::vector<int>& rule_ids_to_remove,
                              const std::vector<TestRule>& rules_to_add,
                              RulesetScope scope,
                              const std::string* expected_error = nullptr) {
    base::Value::List ids_to_remove_value = VectorToList(rule_ids_to_remove);
    base::Value::List rules_to_add_value = ToListValue(rules_to_add);

    constexpr const char kParams[] = R"(
      [{
        "addRules": $1,
        "removeRuleIds": $2
      }]
    )";
    const std::string json_args = content::JsReplace(
        kParams, std::move(rules_to_add_value), std::move(ids_to_remove_value));

    scoped_refptr<ExtensionFunction> update_function;
    switch (scope) {
      case RulesetScope::kDynamic:
        update_function = base::MakeRefCounted<
            DeclarativeNetRequestUpdateDynamicRulesFunction>();
        break;
      case RulesetScope::kSession:
        update_function = base::MakeRefCounted<
            DeclarativeNetRequestUpdateSessionRulesFunction>();
        break;
    }
    update_function->set_extension(&extension);
    update_function->set_has_callback(true);
    if (!expected_error) {
      ASSERT_TRUE(api_test_utils::RunFunction(update_function.get(), json_args,
                                              browser_context()));
      return;
    }

    ASSERT_EQ(*expected_error,
              api_test_utils::RunFunctionAndReturnError(
                  update_function.get(), json_args, browser_context()));
  }

  // Runs getDynamicRules/getSessionRules extension function and populates
  // |result|.
  void RunGetRulesFunction(const Extension& extension,
                           RulesetScope scope,
                           base::Value* result) {
    RunGetRulesFunction(extension, scope, std::nullopt /* rule_ids */, result);
  }

  void RunGetRulesFunction(
      const Extension& extension,
      RulesetScope scope,
      const std::optional<const std::vector<int>>& rule_ids,
      base::Value* result) {
    CHECK(result);

    std::string json_args = "[]";

    if (rule_ids) {
      constexpr const char kParams[] = R"(
        [{
          "ruleIds": $1
        }]
      )";
      base::Value::List rule_ids_value = VectorToList(rule_ids.value());

      json_args = content::JsReplace(kParams, std::move(rule_ids_value));
    }

    scoped_refptr<ExtensionFunction> function;
    switch (scope) {
      case RulesetScope::kDynamic:
        function = base::MakeRefCounted<
            DeclarativeNetRequestGetDynamicRulesFunction>();
        break;
      case RulesetScope::kSession:
        function = base::MakeRefCounted<
            DeclarativeNetRequestGetSessionRulesFunction>();
        break;
    }
    function->set_extension(&extension);
    function->set_has_callback(true);

    auto result_ptr = api_test_utils::RunFunctionAndReturnSingleResult(
        function.get(), json_args, browser_context());
    ASSERT_TRUE(result_ptr);
    ASSERT_TRUE(result_ptr->is_list());
    *result = std::move(*result_ptr);
  }

  void RunUpdateEnabledRulesetsFunction(
      const Extension& extension,
      const std::vector<std::string>& ruleset_ids_to_remove,
      const std::vector<std::string>& ruleset_ids_to_add,
      std::optional<std::string> expected_error) {
    base::Value::List ids_to_remove_value = ToListValue(ruleset_ids_to_remove);
    base::Value::List ids_to_add_value = ToListValue(ruleset_ids_to_add);

    constexpr const char kParams[] = R"(
      [{
        "disableRulesetIds": $1,
        "enableRulesetIds": $2
      }]
    )";
    const std::string json_args = content::JsReplace(
        kParams, std::move(ids_to_remove_value), std::move(ids_to_add_value));

    auto function = base::MakeRefCounted<
        DeclarativeNetRequestUpdateEnabledRulesetsFunction>();
    function->set_extension(&extension);
    function->set_has_callback(true);

    if (!expected_error) {
      EXPECT_TRUE(api_test_utils::RunFunction(function.get(), json_args,
                                              browser_context()));
      return;
    }

    EXPECT_EQ(expected_error,
              api_test_utils::RunFunctionAndReturnError(
                  function.get(), json_args, browser_context()));
  }

  void VerifyGetEnabledRulesetsFunction(
      const Extension& extension,
      const std::vector<std::string>& expected_ids) {
    auto function =
        base::MakeRefCounted<DeclarativeNetRequestGetEnabledRulesetsFunction>();
    function->set_extension(&extension);
    function->set_has_callback(true);

    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), "[]" /* args */, browser_context());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());

    std::u16string error;
    std::vector<std::string> actual_ids;
    for (const auto& val : result->GetList())
      actual_ids.push_back(val.GetString());

    EXPECT_THAT(expected_ids, UnorderedElementsAreArray(actual_ids));
  }

  void RunUpdateStaticRulesFunction(const Extension& extension,
                                    const std::string& ruleset_id,
                                    const std::vector<int>& rule_ids_to_disable,
                                    const std::vector<int>& rule_ids_to_enable,
                                    std::optional<std::string> expected_error) {
    base::Value::List ids_to_disable = VectorToList(rule_ids_to_disable);
    base::Value::List ids_to_enable = VectorToList(rule_ids_to_enable);

    constexpr const char kParams[] = R"([{ "rulesetId": $1,
                                           "disableRuleIds": $2,
                                           "enableRuleIds": $3 }])";
    const std::string json_args = content::JsReplace(
        kParams, ruleset_id, base::Value(std::move(ids_to_disable)),
        base::Value(std::move(ids_to_enable)));

    auto function =
        base::MakeRefCounted<DeclarativeNetRequestUpdateStaticRulesFunction>();
    function->set_extension(&extension);
    function->set_has_callback(true);

    if (!expected_error) {
      EXPECT_TRUE(api_test_utils::RunFunction(function.get(), json_args,
                                              browser_context()));
      return;
    }

    EXPECT_EQ(expected_error,
              api_test_utils::RunFunctionAndReturnError(
                  function.get(), json_args, browser_context()));
  }

  base::flat_set<int> GetDisabledRuleIdsFromMatcher(
      const std::string& ruleset_id_string) {
    return GetDisabledRuleIdsFromMatcherForTesting(*manager(), *extension(),
                                                   ruleset_id_string);
  }

  bool RulesetExists(const std::string& ruleset_id_string) {
    const DNRManifestData::ManifestIDToRulesetMap& public_id_map =
        DNRManifestData::GetManifestIDToRulesetMap(*extension());
    return base::Contains(public_id_map, ruleset_id_string);
  }

  void VerifyGetDisabledRuleIdsFunction(
      const Extension& extension,
      const std::string& ruleset_id,
      const std::vector<int>& expected_disabled_rule_ids) {
    constexpr const char kParams[] = R"([{ "rulesetId": $1 }])";
    const std::string json_args = content::JsReplace(kParams, ruleset_id);

    auto function =
        base::MakeRefCounted<DeclarativeNetRequestGetDisabledRuleIdsFunction>();
    function->set_extension(&extension);
    function->set_has_callback(true);

    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), json_args, browser_context());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());

    std::u16string error;
    std::vector<int> actual_disabled_rule_ids;
    for (const auto& val : result->GetList()) {
      actual_disabled_rule_ids.push_back(val.GetInt());
    }

    EXPECT_EQ(expected_disabled_rule_ids, actual_disabled_rule_ids);
  }

  void VerifyGetDisabledRuleIdsFunctionError(
      const Extension& extension,
      const std::string& ruleset_id,
      std::optional<std::string> expected_error) {
    constexpr const char kParams[] = R"([{ "rulesetId": $1 }])";
    const std::string json_args = content::JsReplace(kParams, ruleset_id);

    auto function =
        base::MakeRefCounted<DeclarativeNetRequestGetDisabledRuleIdsFunction>();
    function->set_extension(&extension);
    function->set_has_callback(true);

    EXPECT_EQ(expected_error,
              api_test_utils::RunFunctionAndReturnError(
                  function.get(), json_args, browser_context()));
  }

  size_t GetDisabledStaticRuleCount() const {
    const PrefsHelper helper(*extension_prefs_);
    return helper.GetDisabledStaticRuleCount(extension()->id());
  }

  void VerifyPublicRulesetIDs(
      const Extension& extension,
      const std::vector<std::string>& expected_public_ruleset_ids) {
    CompositeMatcher* matcher =
        manager()->GetMatcherForExtension(extension.id());
    ASSERT_TRUE(matcher);

    EXPECT_THAT(
        expected_public_ruleset_ids,
        UnorderedElementsAreArray(GetPublicRulesetIDs(extension, *matcher)));
  }

  void UpdateExtensionLoaderAndPath(const base::FilePath& file_path) {
    loader_ = CreateExtensionLoader();
    extension_ = nullptr;
    extension_dir_ = file_path;
    ASSERT_TRUE(base::CreateDirectory(extension_dir_));
  }

  void CheckExtensionAllocationInPrefs(
      const ExtensionId& extension_id,
      std::optional<int> expected_rules_count) {
    int actual_rules_count = 0;

    const PrefsHelper helper(*extension_prefs_);
    bool has_allocated_rules_count =
        helper.GetAllocatedGlobalRuleCount(extension_id, actual_rules_count);

    EXPECT_EQ(expected_rules_count.has_value(), has_allocated_rules_count);
    if (expected_rules_count.has_value())
      EXPECT_EQ(*expected_rules_count, actual_rules_count);
  }

  void VerifyGetAvailableStaticRuleCountFunction(
      const Extension& extension,
      size_t expected_available_rule_count) {
    auto function = base::MakeRefCounted<
        DeclarativeNetRequestGetAvailableStaticRuleCountFunction>();
    function->set_extension(&extension);
    function->set_has_callback(true);

    std::optional<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), "[]" /* args */, browser_context());
    ASSERT_TRUE(result);

    EXPECT_EQ(expected_available_rule_count,
              static_cast<size_t>(result->GetInt()));
  }

  ExtensionPrefs* extension_prefs() { return extension_prefs_; }

  ChromeTestExtensionLoader* extension_loader() { return loader_.get(); }

  const Extension* extension() const { return extension_.get(); }

  const base::FilePath& extension_dir() const { return extension_dir_; }

  LoadErrorReporter* error_reporter() {
    return LoadErrorReporter::GetInstance();
  }

 private:
  // Derived classes must override this to persist the extension to disk.
  virtual void WriteExtensionData() = 0;

  base::FilePath extension_dir_;
  std::unique_ptr<ChromeTestExtensionLoader> loader_;
  scoped_refptr<const Extension> extension_;
  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;

  // Override the various API rule limits to prevent a timeout.
  base::AutoReset<int> guaranteed_minimum_override_ =
      CreateScopedStaticGuaranteedMinimumOverrideForTesting(100);
  base::AutoReset<int> global_limit_override_ =
      CreateScopedGlobalStaticRuleLimitOverrideForTesting(200);
  base::AutoReset<int> regex_rule_limit_override_ =
      CreateScopedRegexRuleLimitOverrideForTesting(100);
  base::AutoReset<int> dynamic_rule_limit_override_ =
      CreateScopedDynamicRuleLimitOverrideForTesting(200);
  base::AutoReset<int> unsafe_dynamic_rule_limit_override_ =
      CreateScopedUnsafeDynamicRuleLimitOverrideForTesting(100);
  base::AutoReset<int> session_rule_limit_override_ =
      CreateScopedSessionRuleLimitOverrideForTesting(200);
  base::AutoReset<int> unsafe_session_rule_limit_override_ =
      CreateScopedUnsafeSessionRuleLimitOverrideForTesting(100);
};

// Fixture testing that declarative rules corresponding to the Declarative Net
// Request API are correctly indexed, for both packed and unpacked extensions.
// This only tests a single ruleset.
class SingleRulesetTest : public DeclarativeNetRequestUnittest {
 public:
  SingleRulesetTest() = default;

 protected:
  void AddRule(const TestRule& rule) { rules_list_.push_back(rule); }

  // This takes precedence over the AddRule method.
  void SetRules(base::Value rules) { rules_value_ = std::move(rules); }

  void set_persist_invalid_json_file() { persist_invalid_json_file_ = true; }

  void set_persist_initial_indexed_ruleset() {
    persist_initial_indexed_ruleset_ = true;
  }

  void LoadAndExpectError(const std::string& expected_error) {
    DeclarativeNetRequestUnittest::LoadAndExpectError(expected_error,
                                                      kJSONRulesFilename);
  }

  void LoadAndExpectParseFailure(ParseResult parse_result, int rule_id) {
    DeclarativeNetRequestUnittest::LoadAndExpectParseFailure(
        parse_result, rule_id, kJSONRulesFilename);
  }

  // |expected_rules_count| refers to the count of indexed rules. When
  // |expected_rules_count| is not set, it is inferred from the added rules.
  void LoadAndExpectSuccess(
      const std::optional<size_t>& expected_rules_count = std::nullopt) {
    size_t rules_count = 0;
    if (expected_rules_count)
      rules_count = *expected_rules_count;
    else if (rules_value_ && rules_value_->is_list())
      rules_count = rules_value_->GetList().size();
    else
      rules_count = rules_list_.size();

    // We only index up to GetMaximumRulesPerRuleset() rules per ruleset.
    rules_count =
        std::min(rules_count, static_cast<size_t>(GetMaximumRulesPerRuleset()));
    DeclarativeNetRequestUnittest::LoadAndExpectSuccess(rules_count,
                                                        rules_count, true);
  }

 private:
  // DeclarativeNetRequestUnittest override:
  void WriteExtensionData() override {
    if (!rules_value_)
      rules_value_ = base::Value(ToListValue(rules_list_));

    WriteManifestAndRuleset(
        extension_dir(),
        TestRulesetInfo(kDefaultRulesetID, kJSONRulesFilename,
                        rules_value_->Clone()),
        {} /* hosts */);

    // Overwrite the JSON rules file with some invalid json.
    if (persist_invalid_json_file_) {
      std::string data = "invalid json";
      ASSERT_TRUE(base::WriteFile(
          extension_dir().AppendASCII(kJSONRulesFilename), data));
    }

    if (persist_initial_indexed_ruleset_) {
      std::string data = "user ruleset";
      base::FilePath ruleset_path =
          extension_dir().Append(file_util::GetIndexedRulesetRelativePath(
              kMinValidStaticRulesetID.value()));
      ASSERT_TRUE(base::CreateDirectory(ruleset_path.DirName()));
      ASSERT_TRUE(base::WriteFile(ruleset_path, data));
    }
  }

  std::vector<TestRule> rules_list_;
  std::optional<base::Value> rules_value_;
  bool persist_invalid_json_file_ = false;
  bool persist_initial_indexed_ruleset_ = false;
};

TEST_P(SingleRulesetTest, DuplicateResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types =
      std::vector<std::string>({"image", "stylesheet"});
  rule.condition->excluded_resource_types = std::vector<std::string>({"image"});
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED,
                            *rule.id);
}

TEST_P(SingleRulesetTest, EmptyRedirectRuleUrl) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  AddRule(rule);

  rule.id = kMinValidID + 1;
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  AddRule(rule);

  LoadAndExpectParseFailure(ParseResult::ERROR_INVALID_REDIRECT, *rule.id);
}

TEST_P(SingleRulesetTest, InvalidRuleID) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID - 1;
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_INVALID_RULE_ID, *rule.id);
}

TEST_P(SingleRulesetTest, InvalidRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("https://google.com");
  rule.priority = kMinValidPriority - 1;
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_INVALID_RULE_PRIORITY, *rule.id);
}

TEST_P(SingleRulesetTest, NoApplicableResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_resource_types = std::vector<std::string>(
      {"main_frame", "sub_frame", "stylesheet", "script", "image", "font",
       "object", "xmlhttprequest", "ping", "csp_report", "media", "websocket",
       "webtransport", "webbundle", "other"});
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES,
                            *rule.id);
}

// Ensure that rules with both "domains" and "initiator_domains" conditions fail
// parsing.
TEST_P(SingleRulesetTest, DuplicateDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>({"domain.example"});
  rule.condition->initiator_domains =
      std::vector<std::string>({"domain.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(
      ParseResult::ERROR_DOMAINS_AND_INITIATOR_DOMAINS_BOTH_SPECIFIED,
      *rule.id);
}

// Ensure that rules with both "excluded_domains" and
// "excluded_initiator_domains" conditions fail parsing.
TEST_P(SingleRulesetTest, DuplicateExcludedDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_domains =
      std::vector<std::string>({"domain.example"});
  rule.condition->excluded_initiator_domains =
      std::vector<std::string>({"domain.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(
      ParseResult::
          ERROR_EXCLUDED_DOMAINS_AND_EXCLUDED_INITIATOR_DOMAINS_BOTH_SPECIFIED,
      *rule.id);
}

// Ensure that rules with an empty "domains" condition fail parsing.
TEST_P(SingleRulesetTest, EmptyDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_EMPTY_DOMAINS_LIST, *rule.id);
}

// Ensure that rules with an empty "initiator_domains" condition fail parsing.
TEST_P(SingleRulesetTest, EmptyInitiatorDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->initiator_domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_EMPTY_INITIATOR_DOMAINS_LIST,
                            *rule.id);
}

// Ensure that rules with an empty "request_domains" condition fail parsing.
TEST_P(SingleRulesetTest, EmptyRequestDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->request_domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_EMPTY_REQUEST_DOMAINS_LIST,
                            *rule.id);
}

// Ensure that rules with a "domains" condition that contains non-ascii
// characters fail parsing.
TEST_P(SingleRulesetTest, NonAsciiDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>({"😎.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_NON_ASCII_DOMAIN, *rule.id);
}

// Ensure that rules with a "excluded_domains" condition that contains non-ascii
// characters fail parsing.
TEST_P(SingleRulesetTest, NonAsciiExcludedDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_domains = std::vector<std::string>({"😎.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_NON_ASCII_EXCLUDED_DOMAIN,
                            *rule.id);
}

// Ensure that rules with a "initiator_domains" condition that contains
// non-ascii characters fail parsing.
TEST_P(SingleRulesetTest, NonAsciiInitiatorDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->initiator_domains = std::vector<std::string>({"😎.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_NON_ASCII_INITIATOR_DOMAIN,
                            *rule.id);
}

// Ensure that rules with a "excluded_initiator_domains" condition that contains
// non-ascii characters fail parsing.
TEST_P(SingleRulesetTest, NonAsciiExcludedInitiatorDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_initiator_domains =
      std::vector<std::string>({"😎.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(
      ParseResult::ERROR_NON_ASCII_EXCLUDED_INITIATOR_DOMAIN, *rule.id);
}

// Ensure that rules with a "request_domains" condition that contains non-ascii
// characters fail parsing.
TEST_P(SingleRulesetTest, NonAsciiRequestDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->request_domains = std::vector<std::string>({"😎.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_NON_ASCII_REQUEST_DOMAIN,
                            *rule.id);
}

// Ensure that rules with a "excluded_request_domains" condition that contains
// non-ascii characters fail parsing.
TEST_P(SingleRulesetTest, NonAsciiExcludedRequestDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_request_domains =
      std::vector<std::string>({"😎.example"});
  AddRule(rule);
  LoadAndExpectParseFailure(
      ParseResult::ERROR_NON_ASCII_EXCLUDED_REQUEST_DOMAIN, *rule.id);
}

TEST_P(SingleRulesetTest, EmptyResourceTypeList) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST,
                            *rule.id);
}

TEST_P(SingleRulesetTest, EmptyURLFilter) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string();
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_EMPTY_URL_FILTER, *rule.id);
}

TEST_P(SingleRulesetTest, InvalidRedirectURL) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("google");
  rule.priority = kMinValidPriority;
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_INVALID_REDIRECT_URL, *rule.id);
}

TEST_P(SingleRulesetTest, ListNotPassed) {
  SetRules(base::Value(base::Value::Dict()));
  LoadAndExpectError(kErrorListNotPassed);
}

TEST_P(SingleRulesetTest, DuplicateIDS) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);
  AddRule(rule);
  LoadAndExpectParseFailure(ParseResult::ERROR_DUPLICATE_IDS, *rule.id);
}

// Ensure that we limit the number of parse failure warnings shown.
TEST_P(SingleRulesetTest, TooManyParseFailures) {
  const size_t kNumInvalidRules = 10;
  const size_t kNumValidRules = 6;
  const size_t kMaxUnparsedRulesWarnings = 5;

  size_t rule_id = kMinValidID;
  for (size_t i = 0; i < kNumInvalidRules; i++) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_id++;
    rule.action->type = std::string("invalid_action_type");
    AddRule(rule);
  }

  for (size_t i = 0; i < kNumValidRules; i++) {
    TestRule rule = CreateGenericRule();
    rule.id = rule_id++;
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(kNumValidRules);

  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    std::vector<InstallWarning> expected_warnings =
        GetFilteredInstallWarnings(*extension());
    ASSERT_EQ(1u + kMaxUnparsedRulesWarnings, expected_warnings.size());

    InstallWarning warning("");
    warning.key = dnr_api::ManifestKeys::kDeclarativeNetRequest;
    warning.specific = dnr_api::DNRInfo::kRuleResources;

    // The initial warnings should correspond to the first
    // |kMaxUnparsedRulesWarnings| rules, which couldn't be parsed.
    for (size_t i = 0; i < kMaxUnparsedRulesWarnings; i++) {
      EXPECT_EQ(expected_warnings[i].key, warning.key);
      EXPECT_EQ(expected_warnings[i].specific, warning.specific);
      EXPECT_THAT(expected_warnings[i].message,
                  ::testing::HasSubstr("Parse error"));
    }

    warning.message = ErrorUtils::FormatErrorMessage(
        GetErrorWithFilename(kTooManyParseFailuresWarning),
        base::NumberToString(kMaxUnparsedRulesWarnings));
    EXPECT_EQ(warning, expected_warnings[kMaxUnparsedRulesWarnings]);
  }
}

// Ensures that rules which can't be parsed are ignored and cause an install
// warning.
TEST_P(SingleRulesetTest, InvalidJSONRules_StrongTypes) {
  {
    TestRule rule = CreateGenericRule();
    rule.id = 1;
    AddRule(rule);
  }

  {
    TestRule rule = CreateGenericRule();
    rule.id = 2;
    rule.action->type = std::string("invalid action");
    AddRule(rule);
  }

  {
    TestRule rule = CreateGenericRule();
    rule.id = 3;
    AddRule(rule);
  }

  {
    TestRule rule = CreateGenericRule();
    rule.id = 4;
    rule.condition->domain_type = std::string("invalid_domain_type");
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(2u);

  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    std::vector<InstallWarning> install_warnings =
        GetFilteredInstallWarnings(*extension());
    ASSERT_EQ(2u, install_warnings.size());
    std::vector<InstallWarning> expected_warnings;

    for (const auto& warning : install_warnings) {
      EXPECT_EQ(dnr_api::ManifestKeys::kDeclarativeNetRequest, warning.key);
      EXPECT_EQ(dnr_api::DNRInfo::kRuleResources, warning.specific);
      EXPECT_THAT(warning.message, ::testing::HasSubstr("Parse error"));
    }
  }
}

// Ensures that rules which can't be parsed are ignored and cause an install
// warning.
TEST_P(SingleRulesetTest, InvalidJSONRules_Parsed) {
  const char* kRules = R"(
    [
      {
        "id" : 1,
        "priority": 1,
        "condition" : [],
        "action" : {"type" : "block" }
      },
      {
        "id" : 2,
        "priority": 1,
        "condition" : {"urlFilter" : "abc"},
        "action" : {"type" : "block" }
      },
      {
        "id" : 3,
        "priority": 1,
        "invalidKey" : "invalidKeyValue",
        "condition" : {"urlFilter" : "example"},
        "action" : {"type" : "block" }
      },
      {
        "id" : "6",
        "priority": 1,
        "condition" : {"urlFilter" : "google"},
        "action" : {"type" : "block" }
      }
    ]
  )";
  SetRules(*base::JSONReader::Read(kRules));

  extension_loader()->set_ignore_manifest_warnings(true);

  // Rules with ids "2" and "3" will be successfully indexed. Note that for rule
  // with id "3", the "invalidKey" will simply be ignored during parsing
  // (without causing any install warning).
  size_t expected_rule_count = 2;
  LoadAndExpectSuccess(expected_rule_count);

  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    std::vector<InstallWarning> install_warnings =
        GetFilteredInstallWarnings(*extension());
    ASSERT_EQ(2u, install_warnings.size());
    std::vector<InstallWarning> expected_warnings;

    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "id 1",
            "'condition': expected dictionary, got list"),
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);
    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "index 4",
            "'id': expected id, got string"),
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);
    EXPECT_EQ(expected_warnings, install_warnings);
  }
}

// Ensure that regex rules which exceed the per rule memory limit are ignored
// and raise an install warning.
TEST_P(SingleRulesetTest, LargeRegexIgnored) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter.reset();
  int id = kMinValidID;

  const int kNumSmallRegex = 5;
  std::string small_regex = kSmallRegexFilter;
  for (int i = 0; i < kNumSmallRegex; i++, id++) {
    rule.id = id;
    rule.condition->regex_filter = small_regex;
    AddRule(rule);
  }

  const int kNumLargeRegex = 2;
  for (int i = 0; i < kNumLargeRegex; i++, id++) {
    rule.id = id;
    rule.condition->regex_filter = kLargeRegexFilter;
    AddRule(rule);
  }

  base::HistogramTester tester;
  extension_loader()->set_ignore_manifest_warnings(true);

  LoadAndExpectSuccess(kNumSmallRegex);

  tester.ExpectBucketCount(kIsLargeRegexHistogram, true, kNumLargeRegex);
  tester.ExpectBucketCount(kIsLargeRegexHistogram, false, kNumSmallRegex);
  tester.ExpectTotalCount(kRegexRuleSizeHistogram,
                          kNumSmallRegex + kNumLargeRegex);

  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    InstallWarning warning_1 = GetLargeRegexWarning(kMinValidID + 5);
    InstallWarning warning_2 = GetLargeRegexWarning(kMinValidID + 6);
    EXPECT_THAT(GetFilteredInstallWarnings(*extension()),
                UnorderedElementsAre(::testing::Eq(std::cref(warning_1)),
                                     ::testing::Eq(std::cref(warning_2))));
  }
}

// Test an extension with both an error and an install warning.
TEST_P(SingleRulesetTest, WarningAndError) {
  // Add a large regex rule which will exceed the per rule memory limit and
  // cause an install warning.
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter.reset();
  rule.id = kMinValidID;
  rule.condition->regex_filter = kLargeRegexFilter;
  AddRule(rule);

  // Add a regex rule with a syntax error.
  rule.condition->regex_filter = "abc(";
  rule.id = kMinValidID + 1;
  AddRule(rule);

  LoadAndExpectParseFailure(ParseResult::ERROR_INVALID_REGEX_FILTER,
                            kMinValidID + 1);
}

// Ensure that we get an install warning on exceeding the regex rule count
// limit.
TEST_P(SingleRulesetTest, RegexRuleCountExceeded) {
  TestRule regex_rule = CreateRegexRule();
  int rule_id = kMinValidID;
  for (int i = 1; i <= GetRegexRuleLimit() + 5; ++i, ++rule_id) {
    regex_rule.id = rule_id;
    regex_rule.condition->regex_filter = base::NumberToString(i);
    AddRule(regex_rule);
  }

  const int kCountNonRegexRules = 5;
  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= kCountNonRegexRules; i++, ++rule_id) {
    rule.id = rule_id;
    rule.condition->url_filter = base::NumberToString(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(GetRegexRuleLimit() + kCountNonRegexRules);
  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    std::vector<InstallWarning> install_warnings =
        GetFilteredInstallWarnings(*extension());

    ASSERT_EQ(1u, install_warnings.size());
    EXPECT_EQ(InstallWarning(GetErrorWithFilename(kRegexRuleCountExceeded),
                             dnr_api::ManifestKeys::kDeclarativeNetRequest,
                             dnr_api::DNRInfo::kRuleResources),
              install_warnings[0]);
  }
}

TEST_P(SingleRulesetTest, InvalidJSONFile) {
  set_persist_invalid_json_file();
  // The error is returned by the JSON parser we use. Hence just test an error
  // is raised.
  LoadAndExpectError("");
}

TEST_P(SingleRulesetTest, EmptyRuleset) {
  LoadAndExpectSuccess();
}

TEST_P(SingleRulesetTest, AddSingleRule) {
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess();
}

TEST_P(SingleRulesetTest, AddTwoRules) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);

  rule.id = kMinValidID + 1;
  AddRule(rule);
  LoadAndExpectSuccess();
}

// Test that we do not use an extension provided indexed ruleset.
TEST_P(SingleRulesetTest, ExtensionWithIndexedRuleset) {
  set_persist_initial_indexed_ruleset();
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess();
}

// Test for crbug.com/931967. Ensures that adding dynamic rules in the midst of
// an initial ruleset load (in response to OnExtensionLoaded) behaves
// predictably and doesn't DCHECK.
TEST_P(SingleRulesetTest, DynamicRulesetRace) {
  RulesetManagerObserver ruleset_waiter(manager());

  AddRule(CreateGenericRule());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  const ExtensionId extension_id = extension()->id();
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  // Simulate indexed ruleset format version change. This will cause a re-index
  // on subsequent extension load. Since this will further delay the initial
  // ruleset load, it helps test that the ruleset loading doesn't race with
  // updating dynamic rules.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  TestExtensionRegistryObserver registry_observer(registry());

  service()->EnableExtension(extension_id);
  scoped_refptr<const Extension> extension =
      registry_observer.WaitForExtensionLoaded();
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension_id, extension->id());

  // At this point, the ruleset will still be loading.
  ASSERT_FALSE(manager()->GetMatcherForExtension(extension_id));

  // Add some dynamic rules.
  std::vector<TestRule> dynamic_rules({CreateGenericRule()});
  ASSERT_NO_FATAL_FAILURE(
      RunUpdateRulesFunction(*extension, {} /* rule_ids_to_remove */,
                             dynamic_rules, RulesetScope::kDynamic));

  // The API function to update the dynamic ruleset should only complete once
  // the initial ruleset loading (in response to OnExtensionLoaded) is complete.
  // Hence by now, both the static and dynamic matchers must be loaded.
  VerifyPublicRulesetIDs(*extension,
                         {kDefaultRulesetID, dnr_api::DYNAMIC_RULESET_ID});
}

// Ensures that an updateEnabledRulesets call in the midst of an initial ruleset
// load (in response to OnExtensionLoaded) behaves predictably and doesn't
// DCHECK.
TEST_P(SingleRulesetTest, UpdateEnabledRulesetsRace) {
  RulesetManagerObserver ruleset_waiter(manager());

  AddRule(CreateGenericRule());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  const ExtensionId extension_id = extension()->id();
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  // Simulate indexed ruleset format version change. This will cause a re-index
  // on subsequent extension load. Since this will further delay the initial
  // ruleset load, it helps test that the ruleset loading doesn't race with the
  // updateEnabledRulesets call.
  ScopedIncrementRulesetVersion scoped_version_change =
      CreateScopedIncrementRulesetVersionForTesting();

  TestExtensionRegistryObserver registry_observer(registry());
  service()->EnableExtension(extension_id);
  scoped_refptr<const Extension> extension =
      registry_observer.WaitForExtensionLoaded();
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension_id, extension->id());

  // At this point, the ruleset will still be loading.
  ASSERT_FALSE(manager()->GetMatcherForExtension(extension_id));

  // Disable the sole extension ruleset.
  RunUpdateEnabledRulesetsFunction(*extension, {kDefaultRulesetID}, {},
                                   std::nullopt);

  // Wait for any pending tasks. This isn't actually necessary for this test
  // (there shouldn't be any pending tasks at this point). However still do this
  // to not rely on any task ordering assumption.
  content::RunAllTasksUntilIdle();

  // The API function to update the enabled rulesets should only complete after
  // the initial ruleset loading (in response to OnExtensionLoaded) is complete.
  // Hence by now, the extension shouldn't have any active rulesets.
  VerifyPublicRulesetIDs(*extension, {});
}

// Tests updateSessionRules and getSessionRules extension function calls.
TEST_P(SingleRulesetTest, SessionRules) {
  // Load an extension with an empty static ruleset.
  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kDefaultRulesetID});

  base::Value result(base::Value::Type::LIST);
  RunGetRulesFunction(*extension(), RulesetScope::kSession, &result);
  EXPECT_TRUE(result.GetList().empty());

  TestRule rule_1 = CreateGenericRule();
  rule_1.id = 1;
  TestRule rule_2 = CreateGenericRule();
  rule_2.id = 2;
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), {}, {rule_1, rule_2}, RulesetScope::kSession));
  RunGetRulesFunction(*extension(), RulesetScope::kSession, &result);
  base::Value::Dict rule_1_value = rule_1.ToValue();
  base::Value::Dict rule_2_value = rule_2.ToValue();
  EXPECT_THAT(result.GetList(), ::testing::UnorderedElementsAre(
                                    ::testing::Eq(std::cref(rule_1_value)),
                                    ::testing::Eq(std::cref(rule_2_value))));
  VerifyPublicRulesetIDs(*extension(),
                         {kDefaultRulesetID, dnr_api::SESSION_RULESET_ID});

  // No rule ID filter specified, return all rules.
  RunGetRulesFunction(*extension(), RulesetScope::kSession, std::nullopt,
                      &result);
  EXPECT_THAT(result.GetList(), ::testing::UnorderedElementsAre(
                                    ::testing::Eq(std::cref(rule_1_value)),
                                    ::testing::Eq(std::cref(rule_2_value))));
  // Empty rule ID filter, return no rules.
  RunGetRulesFunction(*extension(), RulesetScope::kSession,
                      std::make_optional<std::vector<int>>({}), &result);
  EXPECT_THAT(result.GetList(), ::testing::IsEmpty());
  // Rule ID filter includes both rules, return them both.
  RunGetRulesFunction(*extension(), RulesetScope::kSession,
                      std::make_optional<std::vector<int>>({1, 2, 3, 4}),
                      &result);
  EXPECT_THAT(result.GetList(), ::testing::UnorderedElementsAre(
                                    ::testing::Eq(std::cref(rule_1_value)),
                                    ::testing::Eq(std::cref(rule_2_value))));
  // Rule ID filter only matches rule 1, return that.
  RunGetRulesFunction(*extension(), RulesetScope::kSession,
                      std::make_optional<std::vector<int>>({1}), &result);
  EXPECT_THAT(result.GetList(), ::testing::UnorderedElementsAre(
                                    ::testing::Eq(std::cref(rule_1_value))));
  // Rule ID filter only matches rule 2, return that.
  RunGetRulesFunction(*extension(), RulesetScope::kSession,
                      std::make_optional<std::vector<int>>({2}), &result);
  EXPECT_THAT(result.GetList(), ::testing::UnorderedElementsAre(
                                    ::testing::Eq(std::cref(rule_2_value))));
  // Rule ID filter doesn't match any rules, return no rules.
  RunGetRulesFunction(*extension(), RulesetScope::kSession,
                      std::make_optional<std::vector<int>>({3}), &result);
  EXPECT_THAT(result.GetList(), ::testing::IsEmpty());

  // No dynamic rules should be returned.
  RunGetRulesFunction(*extension(), RulesetScope::kDynamic, &result);
  EXPECT_TRUE(result.GetList().empty());

  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(*extension(), {*rule_2.id}, {},
                                                 RulesetScope::kSession));
  RunGetRulesFunction(*extension(), RulesetScope::kSession, &result);
  rule_1_value = rule_1.ToValue();
  EXPECT_THAT(result.GetList(), ::testing::UnorderedElementsAre(
                                    ::testing::Eq(std::cref(rule_1_value))));
  RunGetRulesFunction(*extension(), RulesetScope::kDynamic, &result);
  EXPECT_TRUE(result.GetList().empty());
}

// Ensure an error is raised when an extension adds a session-scoped regex rule
// which consumes more memory than allowed.
TEST_P(SingleRulesetTest, LargeRegexError_SessionRules) {
  // Load an extension with an empty static ruleset.
  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // Ensure adding a normal regex rule succeeds.
  TestRule normal_regex_rule = CreateGenericRule(1);
  normal_regex_rule.condition->url_filter.reset();
  normal_regex_rule.condition->regex_filter = std::string(kSmallRegexFilter);
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), {}, {normal_regex_rule}, RulesetScope::kSession));

  // Ensure an error is raised on adding a large regex rule.
  TestRule large_regex_rule = CreateGenericRule(2);
  large_regex_rule.condition->url_filter.reset();
  large_regex_rule.condition->regex_filter = std::string(kLargeRegexFilter);
  std::string expected_error =
      ErrorUtils::FormatErrorMessage(kErrorRegexTooLarge, "2", kRegexFilterKey);
  ASSERT_NO_FATAL_FAILURE(
      RunUpdateRulesFunction(*extension(), {}, {large_regex_rule},
                             RulesetScope::kSession, &expected_error));
}

// Ensure that we can add up to the |dnr_api::GUARANTEED_MINIMUM_STATIC_RULES| +
// |kMaxStaticRulesPerProfile| rules if the global rules feature is enabled.
TEST_P(SingleRulesetTest, RuleCountLimitMatched) {
  TestRule rule = CreateGenericRule();
  for (int i = 0; i < GetMaximumRulesPerRuleset(); ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = base::NumberToString(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess(300);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension(), FileBackedRulesetSource::RulesetFilter::kIncludeAll);

  ASSERT_EQ(1u, static_sources.size());
  EXPECT_TRUE(base::PathExists(static_sources[0].indexed_path()));

  const PrefsHelper helper(*extension_prefs());
  // The ruleset's ID should not be marked as ignored in prefs.
  EXPECT_FALSE(
      helper.ShouldIgnoreRuleset(extension()->id(), static_sources[0].id()));
}

// Ensure that an extension's allocation will be kept or released when it is
// disabled based on the reason.
TEST_P(SingleRulesetTest, AllocationWhenDisabled) {
  TestRule rule = CreateGenericRule();
  for (int i = 0; i < GetMaximumRulesPerRuleset(); ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = base::NumberToString(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess(300);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // The 200 rules that contribute to the global pool should be tracked.
  GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(200u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // An entry for these 200 rules should be persisted for the extension in
  // prefs.
  CheckExtensionAllocationInPrefs(extension()->id(), 200);

  service()->DisableExtension(extension()->id(),
                              disable_reason::DISABLE_GREYLIST);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  // The extension's last known extra rule count should be persisted after it is
  // disabled.
  EXPECT_EQ(200u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(extension()->id(), 200);

  // Now re-enable the extension. The extension should load all of its rules
  // without any problems.
  service()->EnableExtension(extension()->id());
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  EXPECT_EQ(200u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(extension()->id(), 200);

  // Disable the extension via user action. This should release its allocation.
  service()->DisableExtension(extension()->id(),
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  EXPECT_EQ(0u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(extension()->id(), std::nullopt);
}

// Ensure that we get an install warning on exceeding the rule count limit and
// that no rules are indexed.
TEST_P(SingleRulesetTest, RuleCountLimitExceeded) {
  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= GetMaximumRulesPerRuleset() + 1; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = base::NumberToString(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      0, 0, false /* expect_rulesets_indexed */);

  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension(), FileBackedRulesetSource::RulesetFilter::kIncludeAll);

  // Since the ruleset was ignored and not indexed, it should not be persisted
  // to a file.
  ASSERT_EQ(1u, static_sources.size());
  EXPECT_FALSE(base::PathExists(static_sources[0].indexed_path()));

  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    std::vector<InstallWarning> install_warnings =
        GetFilteredInstallWarnings(*extension());
    ASSERT_EQ(1u, install_warnings.size());
    InstallWarning expected_warning = InstallWarning(
        GetErrorWithFilename(ErrorUtils::FormatErrorMessage(
            kIndexingRuleLimitExceeded,
            base::NumberToString(static_sources[0].id().value()))),
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);

    EXPECT_EQ(expected_warning, install_warnings[0]);
  }

  const PrefsHelper helper(*extension_prefs());

  // The ruleset's ID should be persisted in the ignored rulesets pref.
  EXPECT_TRUE(
      helper.ShouldIgnoreRuleset(extension()->id(), static_sources[0].id()));

  // Since the ruleset was not indexed, no rules should contribute to the extra
  // static rule count.
  GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(0u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // Likewise, no entry should be persisted in prefs.
  CheckExtensionAllocationInPrefs(extension()->id(), std::nullopt);
}

// Tests that rule limits for both rule count and unsafe rule count are enforced
// for the dynamic and session rulesets of an extension.
TEST_P(SingleRulesetTest, DynamicAndSessionRuleLimits) {
  ASSERT_EQ(200, GetDynamicRuleLimit());
  ASSERT_EQ(100, GetUnsafeDynamicRuleLimit());
  ASSERT_EQ(200, GetSessionRuleLimit());
  ASSERT_EQ(100, GetUnsafeSessionRuleLimit());

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // `rules_1` and `rules_2` contain 50 rules each. They are kept in two lists
  // since they'll be added at different times and all rules between the two
  // lists have unique IDs.
  std::vector<TestRule> rules_1;
  std::vector<TestRule> rules_2;
  int rule_id = kMinValidID;
  for (size_t i = 0; i < 50; ++i) {
    rules_1.push_back(CreateGenericRule(rule_id++));
    rules_2.push_back(CreateGenericRule(rule_id++));
  }

  auto create_unsafe_rule = [](int rule_id) {
    TestRule unsafe_rule = CreateGenericRule(rule_id);

    // Redirect rules are considered "unsafe" in this context.
    unsafe_rule.action->type = std::string("redirect");
    unsafe_rule.action->redirect.emplace();
    unsafe_rule.action->redirect->url = std::string("https://google.com");
    return unsafe_rule;
  };

  std::vector<TestRule> unsafe_rules;
  for (size_t i = 0; i < 100; ++i) {
    unsafe_rules.push_back(create_unsafe_rule(rule_id++));
  }

  // Run the same test for both dynamic and session rulesets. Note that rule
  // counts for ordinary and "unsafe" rules are separately tracked for each
  // ruleset.
  struct {
    RulesetScope ruleset_type;
    std::string rule_count_exceeded_error;
    std::string unsafe_rule_count_exceeded_error;
  } test_cases[] = {
      {RulesetScope::kDynamic, kDynamicRuleCountExceeded,
       kDynamicUnsafeRuleCountExceeded},
      {RulesetScope::kSession, kSessionRuleCountExceeded,
       kSessionUnsafeRuleCountExceeded},
  };

  const RulesMonitorService* service =
      RulesMonitorService::Get(browser_context());
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Testing %s ruleset:", test_case.ruleset_type == RulesetScope::kDynamic
                                   ? "dynamic"
                                   : "session"));
    RulesetID ruleset_id = test_case.ruleset_type == RulesetScope::kDynamic
                               ? kDynamicRulesetID
                               : kSessionRulesetID;

    // Add some ordinary rules first; this should succeed.
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), /*rule_ids_to_remove=*/{}, rules_1,
                               test_case.ruleset_type));
    RuleCounts expected_rule_counts(/*rule_count=*/50, /*unsafe_rule_count=*/0,
                                    /*regex_rule_count=*/0);
    EXPECT_EQ(expected_rule_counts,
              service->GetRuleCounts(extension()->id(), ruleset_id));

    // Now add "unsafe" rules up to the unsafe rule limit; this should succeed.
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), /*rule_ids_to_remove=*/{},
                               unsafe_rules, test_case.ruleset_type));
    expected_rule_counts =
        RuleCounts(/*rule_count=*/150, /*unsafe_rule_count=*/100,
                   /*regex_rule_count=*/0);
    EXPECT_EQ(expected_rule_counts,
              service->GetRuleCounts(extension()->id(), ruleset_id));

    // Adding any more "unsafe" rules should result in an error.
    std::string expected_error = test_case.unsafe_rule_count_exceeded_error;
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), {} /* rule_ids_to_remove */,
                               {create_unsafe_rule(rule_id++)},
                               test_case.ruleset_type, &expected_error));

    // Add ordinary rules up to the safe rule limit; this should succeed.
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), /*rule_ids_to_remove=*/{}, rules_2,
                               test_case.ruleset_type));
    expected_rule_counts =
        RuleCounts(/*rule_count=*/200, /*unsafe_rule_count=*/100,
                   /*regex_rule_count=*/0);
    EXPECT_EQ(expected_rule_counts,
              service->GetRuleCounts(extension()->id(), ruleset_id));

    // Adding any more ordinary rules should result in an error.
    expected_error = test_case.rule_count_exceeded_error;
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), {} /* rule_ids_to_remove */,
                               {CreateGenericRule(rule_id++)},
                               test_case.ruleset_type, &expected_error));
  }
}

class SingleRulesetWithoutSafeRulesTest : public SingleRulesetTest {
 public:
  SingleRulesetWithoutSafeRulesTest() {
    scoped_feature_list_.InitAndDisableFeature(
        extensions_features::kDeclarativeNetRequestSafeRuleLimits);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40282671): This is just a sanity check that rule counts work
// as intended when the safe rules feature flag is turned off. Remove this test
// once feature is enabled and the feature flag is removed.
TEST_P(SingleRulesetWithoutSafeRulesTest, DynamicAndSessionRuleLimits) {
  // With `kDeclarativeNetRequestSafeRuleLimits` disabled, the dynamic/session
  // rule limits are equal to their unsafe rule limit versions.
  ASSERT_EQ(100, GetDynamicRuleLimit());
  ASSERT_EQ(100, GetUnsafeDynamicRuleLimit());
  ASSERT_EQ(100, GetSessionRuleLimit());
  ASSERT_EQ(100, GetUnsafeSessionRuleLimit());

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  std::vector<TestRule> rules;
  int rule_id = kMinValidID;
  for (size_t i = 0; i < 100; ++i) {
    // Redirect rules are considered "unsafe" in this context.
    TestRule rule = CreateGenericRule(rule_id++);
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->url = std::string("https://google.com");
    rules.push_back(rule);
  }

  // Run the same test for both dynamic and session rulesets. Note that rule
  // counts are separately tracked for each ruleset.
  struct {
    RulesetScope ruleset_type;
    std::string rule_count_exceeded_error;
    std::string unsafe_rule_count_exceeded_error;
  } test_cases[] = {
      {RulesetScope::kDynamic, kDynamicRuleCountExceeded},
      {RulesetScope::kSession, kSessionRuleCountExceeded},
  };

  const RulesMonitorService* service =
      RulesMonitorService::Get(browser_context());
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        "Testing %s ruleset:", test_case.ruleset_type == RulesetScope::kDynamic
                                   ? "dynamic"
                                   : "session"));
    RulesetID ruleset_id = test_case.ruleset_type == RulesetScope::kDynamic
                               ? kDynamicRulesetID
                               : kSessionRulesetID;

    // Add rules up to the rule limit.
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), /*rule_ids_to_remove=*/{}, rules,
                               test_case.ruleset_type));
    RuleCounts expected_rule_counts(/*rule_count=*/100,
                                    /*unsafe_rule_count=*/100,
                                    /*regex_rule_count=*/0);
    EXPECT_EQ(expected_rule_counts,
              service->GetRuleCounts(extension()->id(), ruleset_id));

    // Adding any more rules should result in an error. Note that since the
    // `kDeclarativeNetRequestSafeRuleLimits` feature is disabled for this test,
    // the error returned should only mention that the rule count has been
    // exceeded (see `test_cases`).
    std::string expected_error = test_case.rule_count_exceeded_error;
    ASSERT_NO_FATAL_FAILURE(
        RunUpdateRulesFunction(*extension(), {} /* rule_ids_to_remove */,
                               {CreateGenericRule(rule_id++)},
                               test_case.ruleset_type, &expected_error));
  }
}

// Tests that the regex rule limit is correctly shared between dynamic and
// session rulesets of an extension.
TEST_P(SingleRulesetTest, SharedDynamicAndSessionRegexRuleLimits) {
  ASSERT_EQ(100, GetRegexRuleLimit());

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // Add 50 session-scoped regex rules, along with 10 non-regex rules.
  std::vector<TestRule> session_rules;
  int rule_id = kMinValidID;
  for (size_t i = 0; i < 50; ++i)
    session_rules.push_back(CreateRegexRule(rule_id++));
  for (size_t i = 0; i < 10; ++i)
    session_rules.push_back(CreateGenericRule(rule_id++));

  ASSERT_NO_FATAL_FAILURE(
      RunUpdateRulesFunction(*extension(), /*rule_ids_to_remove=*/{},
                             session_rules, RulesetScope::kSession));

  // Add the same number of dynamic rules, it should succeed as well.
  std::vector<TestRule> dynamic_rules = session_rules;
  ASSERT_NO_FATAL_FAILURE(
      RunUpdateRulesFunction(*extension(), /*rule_ids_to_remove=*/{},
                             dynamic_rules, RulesetScope::kDynamic));

  const RulesMonitorService* service =
      RulesMonitorService::Get(browser_context());
  RuleCounts expected_count(/*rule_count=*/60, /*unsafe_rule_count=*/0,
                            /*regex_rule_count=*/50);
  EXPECT_EQ(expected_count,
            service->GetRuleCounts(extension()->id(), kDynamicRulesetID));
  EXPECT_EQ(expected_count,
            service->GetRuleCounts(extension()->id(), kSessionRulesetID));

  // Adding more regex based dynamic or session rules should fail.
  std::string expected_error = kDynamicRegexRuleCountExceeded;
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), /*rule_ids_to_remove=*/{}, {CreateRegexRule(rule_id++)},
      RulesetScope::kDynamic, &expected_error));
  expected_error = kSessionRegexRuleCountExceeded;
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), /*rule_ids_to_remove=*/{}, {CreateRegexRule(rule_id++)},
      RulesetScope::kSession, &expected_error));

  // Adding non-regex dynamic or session rules should still succeed.
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), /*rule_ids_to_remove=*/{}, {CreateGenericRule(rule_id++)},
      RulesetScope::kDynamic));
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), /*rule_ids_to_remove=*/{}, {CreateGenericRule(rule_id++)},
      RulesetScope::kSession));

  expected_count.rule_count++;
  EXPECT_EQ(expected_count,
            service->GetRuleCounts(extension()->id(), kDynamicRulesetID));
  EXPECT_EQ(expected_count,
            service->GetRuleCounts(extension()->id(), kSessionRulesetID));
}

// Test that getMatchedRules will return an error if an invalid tab id is
// specified.
TEST_P(SingleRulesetTest, GetMatchedRulesInvalidTabID) {
  LoadAndExpectSuccess();
  const ExtensionId extension_id = extension()->id();

  auto function =
      base::MakeRefCounted<DeclarativeNetRequestGetMatchedRulesFunction>();
  function->set_extension(extension());

  std::string expected_error = ErrorUtils::FormatErrorMessage(
      declarative_net_request::kTabNotFoundError, "-9001");

  std::string error = api_test_utils::RunFunctionAndReturnError(
      function.get(), R"([{ "tabId": -9001 }])" /* args */, browser_context());

  EXPECT_EQ(expected_error, error);
}

// Tests that multiple static rulesets are correctly indexed.
class MultipleRulesetsTest : public DeclarativeNetRequestUnittest {
 public:
  MultipleRulesetsTest() = default;

 protected:
  // DeclarativeNetRequestUnittest override:
  void WriteExtensionData() override {
    WriteManifestAndRulesets(extension_dir(), rulesets_, {} /* hosts */);
  }

  void AddRuleset(const TestRulesetInfo& info) { rulesets_.push_back(info); }

  void ClearRulesets() { rulesets_.clear(); }

  TestRulesetInfo CreateRuleset(const std::string& manifest_id_and_path,
                                size_t num_non_regex_rules,
                                size_t num_regex_rules,
                                bool enabled) {
    std::vector<TestRule> rules;
    TestRule rule = CreateGenericRule();
    int id = kMinValidID;
    for (size_t i = 0; i < num_non_regex_rules; ++i, ++id) {
      rule.id = id;
      rules.push_back(rule);
    }

    for (size_t i = 0; i < num_regex_rules; ++i, ++id)
      rules.push_back(CreateRegexRule(id));

    return TestRulesetInfo(manifest_id_and_path, ToListValue(rules), enabled);
  }

  // |expected_rules_count| and |expected_enabled_rules_count| refer to the
  // counts of indexed rules. When not set, these are inferred from the added
  // rulesets.
  void LoadAndExpectSuccess(
      const std::optional<size_t>& expected_rules_count = std::nullopt,
      const std::optional<size_t>& expected_enabled_rules_count =
          std::nullopt) {
    size_t static_rule_limit = GetMaximumRulesPerRuleset();
    size_t rules_count = 0u;
    size_t rules_enabled_count = 0u;
    for (const TestRulesetInfo& info : rulesets_) {
      size_t count = info.rules_value.GetList().size();

      // We only index up to |static_rule_limit| rules per ruleset, but
      // may index more rules than this limit across rulesets.
      count = std::min(count, static_rule_limit);

      rules_count += count;
      if (info.enabled)
        rules_enabled_count += count;
    }

    DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
        expected_rules_count.value_or(rules_count),
        expected_enabled_rules_count.value_or(rules_enabled_count),
        !rulesets_.empty());
  }

 private:
  std::vector<TestRulesetInfo> rulesets_;
};

// Tests an extension with multiple static rulesets.
TEST_P(MultipleRulesetsTest, Success) {
  size_t kNumRulesets = 7;
  size_t kRulesPerRuleset = 10;

  for (size_t i = 0; i < kNumRulesets; ++i) {
    AddRuleset(
        CreateRuleset(base::NumberToString(i), kRulesPerRuleset, 0, true));
  }

  LoadAndExpectSuccess();
}

// Tests an extension with no static rulesets.
TEST_P(MultipleRulesetsTest, ZeroRulesets) {
  LoadAndExpectSuccess();
  VerifyGetEnabledRulesetsFunction(*extension(), {});
}

// Tests an extension with multiple empty rulesets.
TEST_P(MultipleRulesetsTest, EmptyRulesets) {
  size_t kNumRulesets = 7;

  for (size_t i = 0; i < kNumRulesets; ++i)
    AddRuleset(CreateRuleset(base::NumberToString(i), 0, 0, true));

  LoadAndExpectSuccess();
}

// Tests an extension with multiple static rulesets, with one of rulesets
// specifying an invalid rules file.
TEST_P(MultipleRulesetsTest, ListNotPassed) {
    std::vector<TestRule> rules({CreateGenericRule()});
    AddRuleset(TestRulesetInfo(kId1, "path1", ToListValue(rules)));

    // Persist a ruleset with an invalid rules file.
    AddRuleset(
        TestRulesetInfo(kId2, "path2", base::Value(base::Value::Type::DICT)));

    AddRuleset(TestRulesetInfo(kId3, "path3", base::Value::List()));

    LoadAndExpectError(kErrorListNotPassed, "path2" /* filename */);
}

// Tests an extension with multiple static rulesets with each ruleset generating
// some install warnings.
TEST_P(MultipleRulesetsTest, InstallWarnings) {
  size_t expected_rule_count = 0;
  size_t enabled_rule_count = 0;
  std::vector<std::string> expected_warnings;
  {
    // Persist a ruleset with an install warning for a large regex.
    std::vector<TestRule> rules;
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rules.push_back(rule);

    rule.id = kMinValidID + 1;
    rule.condition->url_filter.reset();
    rule.condition->regex_filter = kLargeRegexFilter;
    rules.push_back(rule);

    TestRulesetInfo info(kId1, "path1", ToListValue(rules), true);
    AddRuleset(info);

    expected_warnings.push_back(
        GetLargeRegexWarning(*rule.id, info.relative_file_path).message);

    expected_rule_count += rules.size();
    enabled_rule_count += 1;
  }

  {
    // Persist a ruleset with an install warning for exceeding the regex rule
    // count.
    size_t kCountNonRegexRules = 5;
    TestRulesetInfo info = CreateRuleset(kId3, kCountNonRegexRules,
                                         GetRegexRuleLimit() + 1, false);
    AddRuleset(info);

    expected_warnings.push_back(
        GetErrorWithFilename(kRegexRuleCountExceeded, info.relative_file_path));

    expected_rule_count += kCountNonRegexRules + GetRegexRuleLimit();
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(expected_rule_count, enabled_rule_count);

  // TODO(crbug.com/40591637): CrxInstaller reloads the extension after moving
  // it, which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    std::vector<InstallWarning> warnings =
        GetFilteredInstallWarnings(*extension());
    std::vector<std::string> warning_strings;
    for (const InstallWarning& warning : warnings)
      warning_strings.push_back(warning.message);

    EXPECT_THAT(warning_strings, UnorderedElementsAreArray(expected_warnings));
  }
}

TEST_P(MultipleRulesetsTest, EnabledRulesCount) {
  AddRuleset(CreateRuleset(kId1, 100, 10, true));
  AddRuleset(CreateRuleset(kId2, 200, 20, false));
  AddRuleset(CreateRuleset(kId3, 150, 30, true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // Only the first and third rulesets should be enabled.
  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension()->id());
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId1, kId3});

  EXPECT_THAT(composite_matcher->matchers(),
              UnorderedElementsAre(
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 100 + 10)),
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 150 + 30))));
}

// Ensure that exceeding the regex rules limit across rulesets raises a warning.
TEST_P(MultipleRulesetsTest, RegexRuleCountExceeded) {
  // Enabled on load.
  AddRuleset(CreateRuleset(kId1, 210, 50, true));
  // Won't be enabled on load since including it will exceed the regex rule
  // count.
  AddRuleset(CreateRuleset(kId2, 1, GetRegexRuleLimit(), true));
  // Won't be enabled on load since it is disabled by default.
  AddRuleset(CreateRuleset(kId3, 10, 10, false));
  // Enabled on load.
  AddRuleset(CreateRuleset(kId4, 20, 20, true));

  RulesetManagerObserver ruleset_waiter(manager());
  extension_loader()->set_ignore_manifest_warnings(true);

  LoadAndExpectSuccess();

  // Installing the extension causes an install warning since the set of enabled
  // rulesets exceed the regex rules limit.
  if (GetParam() != ExtensionLoadType::PACKED) {
    EXPECT_THAT(GetFilteredInstallWarnings(*extension()),
                UnorderedElementsAre(Field(&InstallWarning::message,
                                           kEnabledRegexRuleCountExceeded)));
  }

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension()->id());
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId1, kId4});

  EXPECT_THAT(composite_matcher->matchers(),
              UnorderedElementsAre(
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 210 + 50)),
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 20 + 20))));
}

TEST_P(MultipleRulesetsTest, UpdateEnabledRulesets_InvalidRulesetID) {
  AddRuleset(CreateRuleset(kId1, 10, 10, true));
  AddRuleset(CreateRuleset(kId2, 10, 10, false));
  AddRuleset(CreateRuleset(kId3, 10, 10, true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  constexpr char kInvalidRulesetId[] = "invalid_id";
  RunUpdateEnabledRulesetsFunction(
      *extension(), {kId1, kInvalidRulesetId}, {},
      ErrorUtils::FormatErrorMessage(kInvalidRulesetIDError,
                                     kInvalidRulesetId));
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3});

  RunUpdateEnabledRulesetsFunction(
      *extension(), {kId1}, {kId2, kInvalidRulesetId},
      ErrorUtils::FormatErrorMessage(kInvalidRulesetIDError,
                                     kInvalidRulesetId));
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3});

  // Ensure we can't enable/disable dynamic or session-scoped rulesets using
  // updateEnabledRulesets.
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), {}, {CreateGenericRule()}, RulesetScope::kDynamic));
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), {}, {CreateGenericRule()}, RulesetScope::kSession));
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3, dnr_api::DYNAMIC_RULESET_ID,
                                        dnr_api::SESSION_RULESET_ID});
  RunUpdateEnabledRulesetsFunction(
      *extension(), {}, {kId2, dnr_api::DYNAMIC_RULESET_ID},
      ErrorUtils::FormatErrorMessage(kInvalidRulesetIDError,
                                     dnr_api::DYNAMIC_RULESET_ID));
  RunUpdateEnabledRulesetsFunction(
      *extension(), {kId1, dnr_api::SESSION_RULESET_ID}, {},
      ErrorUtils::FormatErrorMessage(kInvalidRulesetIDError,
                                     dnr_api::SESSION_RULESET_ID));
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3, dnr_api::DYNAMIC_RULESET_ID,
                                        dnr_api::SESSION_RULESET_ID});
}

// Ensure we correctly enforce the limit on the maximum number of static
// rulesets that can be enabled at a time
TEST_P(MultipleRulesetsTest,
       UpdateEnabledRulesets_EnabledRulesetCountExceeded) {
  int kMaxEnabledRulesetCount =
      api::declarative_net_request::MAX_NUMBER_OF_ENABLED_STATIC_RULESETS;

  std::vector<std::string> ruleset_ids;
  std::vector<std::string> expected_enabled_ruleset_ids;

  // Create kMaxEnabledRulesetCount + 1 rulesets, with all but the last two
  // enabled.
  for (int i = 0; i <= kMaxEnabledRulesetCount; i++) {
    bool enabled = i < kMaxEnabledRulesetCount - 1;
    std::string id = base::StringPrintf("%d.json", i);
    ruleset_ids.push_back(id);
    if (enabled)
      expected_enabled_ruleset_ids.push_back(id);
    AddRuleset(CreateRuleset(id, 1, 1, enabled));
  }

  std::string first_ruleset_id = ruleset_ids[0];
  std::string second_last_ruleset_id = ruleset_ids[kMaxEnabledRulesetCount - 1];
  std::string last_ruleset_id = ruleset_ids[kMaxEnabledRulesetCount];

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // Since we're not yet at our limit of enabled rulesets, enabling one more
  // should succeed.
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {second_last_ruleset_id},
                                   std::nullopt /* expected_error */);
  expected_enabled_ruleset_ids.push_back(second_last_ruleset_id);
  VerifyPublicRulesetIDs(*extension(), expected_enabled_ruleset_ids);

  // We're now at our limit of enabled rulesets, so enabling another should
  // raise an error.
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {last_ruleset_id},
                                   kEnabledRulesetCountExceeded);
  VerifyPublicRulesetIDs(*extension(), expected_enabled_ruleset_ids);

  // Since this ruleset is already enabled, attempting to enable it again
  // shouldn't raise an error (or do anything).
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {second_last_ruleset_id},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), expected_enabled_ruleset_ids);

  // When enabling and disabling a ruleset at the same time, enabling takes
  // precedence. Since we're still at the limit, that should raise an error.
  RunUpdateEnabledRulesetsFunction(*extension(), {last_ruleset_id},
                                   {last_ruleset_id},
                                   kEnabledRulesetCountExceeded);
  VerifyPublicRulesetIDs(*extension(), expected_enabled_ruleset_ids);

  // Since we're disabling one ruleset, enabling another should not exceed the
  // limit.
  RunUpdateEnabledRulesetsFunction(*extension(), {first_ruleset_id},
                                   {last_ruleset_id},
                                   std::nullopt /* expected_error */);
  expected_enabled_ruleset_ids.erase(expected_enabled_ruleset_ids.begin());
  expected_enabled_ruleset_ids.push_back(last_ruleset_id);
  VerifyPublicRulesetIDs(*extension(), expected_enabled_ruleset_ids);
}

TEST_P(MultipleRulesetsTest, UpdateEnabledRulesets_RegexRuleCountExceeded) {
  AddRuleset(CreateRuleset(kId1, 0, 10, false));
  AddRuleset(CreateRuleset(kId2, 0, GetRegexRuleLimit(), true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId1},
                                   kEnabledRulesetsRegexRuleCountExceeded);
  VerifyPublicRulesetIDs(*extension(), {kId2});
}

TEST_P(MultipleRulesetsTest, UpdateEnabledRulesets_InternalError) {
  AddRuleset(CreateRuleset(kId1, 10, 10, true));
  AddRuleset(CreateRuleset(kId2, 10, 10, true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension(), FileBackedRulesetSource::RulesetFilter::kIncludeAll);
  ASSERT_EQ(2u, static_sources.size());

  constexpr char kReindexHistogram[] =
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful";
  {
    // First disable the second ruleset and then delete its indexed ruleset
    // file.
    RunUpdateEnabledRulesetsFunction(*extension(), {kId2}, {}, std::nullopt);
    ASSERT_TRUE(base::DeleteFile(static_sources[1].indexed_path()));

    // Enabling it again should cause re-indexing and succeed in enabling the
    // ruleset.
    base::HistogramTester tester;
    ASSERT_TRUE(base::DeleteFile(static_sources[1].indexed_path()));

    RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {kId2},
                                     std::nullopt);
    VerifyPublicRulesetIDs(*extension(), {kId2});

    tester.ExpectBucketCount(kReindexHistogram, true /*sample*/, 1 /*count*/);

    EXPECT_TRUE(base::PathExists(static_sources[1].indexed_path()));
  }

  {
    // Now delete both the indexed and json ruleset file for the first ruleset.
    // This will prevent enabling the first ruleset since re-indexing will fail.
    base::HistogramTester tester;
    ASSERT_TRUE(base::DeleteFile(static_sources[0].indexed_path()));
    ASSERT_TRUE(base::DeleteFile(static_sources[0].json_path()));

    RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId1},
                                     kInternalErrorUpdatingEnabledRulesets);
    VerifyPublicRulesetIDs(*extension(), {kId2});

    tester.ExpectBucketCount(kReindexHistogram, false /*sample*/, 1 /*count*/);
  }
}

TEST_P(MultipleRulesetsTest, UpdateAndGetEnabledRulesets_Success) {
  AddRuleset(CreateRuleset(kId1, 10, 10, true));
  AddRuleset(CreateRuleset(kId2, 10, 10, false));
  AddRuleset(CreateRuleset(kId3, 10, 10, true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  RunUpdateEnabledRulesetsFunction(*extension(), {kId1, kId3}, {kId2},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2});

  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId3, kId3},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Ensure no-op calls succeed.
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId2, kId3},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Add dynamic and session-scoped rules and ensure that the setEnabledRulesets
  // call doesn't have any effect on their associated rulesets. Also ensure that
  // the getEnabledRulesets call excludes these rulesets.
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), {}, {CreateGenericRule()}, RulesetScope::kDynamic));
  ASSERT_NO_FATAL_FAILURE(RunUpdateRulesFunction(
      *extension(), {}, {CreateGenericRule()}, RulesetScope::kSession));
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3, dnr_api::DYNAMIC_RULESET_ID,
                                        dnr_api::SESSION_RULESET_ID});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Ensure enabling a ruleset takes priority over disabling.
  RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {kId1},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(),
                         {kId1, kId2, kId3, dnr_api::DYNAMIC_RULESET_ID,
                          dnr_api::SESSION_RULESET_ID});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId1, kId2, kId3});

  // Ensure the set of enabled rulesets persists across extension reloads.
  const ExtensionId extension_id = extension()->id();
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  service()->EnableExtension(extension_id);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  const Extension* extension =
      registry()->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  VerifyPublicRulesetIDs(*extension,
                         {kId1, kId2, kId3, dnr_api::DYNAMIC_RULESET_ID,
                          dnr_api::SESSION_RULESET_ID});
  VerifyGetEnabledRulesetsFunction(*extension, {kId1, kId2, kId3});
}

// Ensure that only rulesets which exceed the rules count limit will not have
// their rules indexed and will raise an install warning.
TEST_P(MultipleRulesetsTest, StaticRuleCountExceeded) {
  // Ruleset should not be indexed as it exceeds the limit.
  AddRuleset(CreateRuleset(kId1, 301, 0, true));

  // Ruleset should be indexed as it is within the limit.
  AddRuleset(CreateRuleset(kId2, 250, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());
  extension_loader()->set_ignore_manifest_warnings(true);

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      250, 250, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension()->id());
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId2});

  EXPECT_THAT(composite_matcher->matchers(),
              UnorderedElementsAre(
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 250))));

  std::vector<FileBackedRulesetSource> static_sources =
      FileBackedRulesetSource::CreateStatic(
          *extension(), FileBackedRulesetSource::RulesetFilter::kIncludeAll);
  ASSERT_EQ(2u, static_sources.size());

  if (GetParam() != ExtensionLoadType::PACKED) {
    std::string expected_warning = GetErrorWithFilename(
        ErrorUtils::FormatErrorMessage(
            kIndexingRuleLimitExceeded,
            base::NumberToString(static_sources[0].id().value())),
        kId1);

    EXPECT_THAT(GetFilteredInstallWarnings(*extension()),
                UnorderedElementsAre(
                    Field(&InstallWarning::message, expected_warning)));
  }

  // Since the first ruleset was ignored and not indexed, it should not be
  // persisted to a file.
  EXPECT_FALSE(base::PathExists(static_sources[0].indexed_path()));

  // The second ruleset was indexed and it should be persisted.
  EXPECT_TRUE(base::PathExists(static_sources[1].indexed_path()));

  const PrefsHelper helper(*extension_prefs());

  // The first ruleset's ID should be persisted in the ignored rulesets pref.
  EXPECT_TRUE(
      helper.ShouldIgnoreRuleset(extension()->id(), static_sources[0].id()));

  // The second ruleset's ID should not be marked as ignored in prefs.
  EXPECT_FALSE(
      helper.ShouldIgnoreRuleset(extension()->id(), static_sources[1].id()));
}

// Ensure that a ruleset which causes the extension to go over the global rule
// limit is correctly ignored.
TEST_P(MultipleRulesetsTest, RulesetIgnored) {
  AddRuleset(CreateRuleset(kId1, 90, 0, true));
  AddRuleset(CreateRuleset(kId2, 150, 0, true));

  // This ruleset should not be loaded because it would exceed the global limit.
  AddRuleset(CreateRuleset(kId3, 100, 0, true));

  AddRuleset(CreateRuleset(kId4, 60, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  // This logs the number of rules the extension has specified to be enabled in
  // the manifest, which may be different than the actual number of rules
  // enabled.
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      400, 400, true /* expect_rulesets_indexed */);

  ExtensionId extension_id = extension()->id();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId1, kId2, kId4});

  EXPECT_THAT(composite_matcher->matchers(),
              UnorderedElementsAre(
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 90)),
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 150)),
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 60))));

  // 200 rules should contribute to the global pool.
  const GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(200u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // Check that the extra static rule count is also persisted in prefs.
  CheckExtensionAllocationInPrefs(extension_id, 200);
}

// Ensure that the global rule count is counted correctly for multiple
// extensions.
TEST_P(MultipleRulesetsTest, MultipleExtensions) {
  // Load an extension with 90 rules.
  AddRuleset(CreateRuleset(kId1, 90, 0, true));
  RulesetManagerObserver ruleset_waiter(manager());

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      90, 90, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1});
  scoped_refptr<const Extension> first_extension = extension();
  ASSERT_TRUE(first_extension.get());

  // The first extension should not have any rules count towards the global
  // pool.
  const GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(0u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // Load an extension with 201 rules.
  UpdateExtensionLoaderAndPath(
      temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension_2")));
  ClearRulesets();
  AddRuleset(CreateRuleset(kId2, 201, 0, true));
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      201, 201, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(2);
  VerifyPublicRulesetIDs(*extension(), {kId2});
  scoped_refptr<const Extension> second_extension = extension();
  ASSERT_TRUE(second_extension.get());

  // The second extension should have 101 rules count towards the global pool.
  EXPECT_EQ(101u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // Load an extension with 150 rules.
  UpdateExtensionLoaderAndPath(
      temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension_3")));
  ClearRulesets();
  AddRuleset(CreateRuleset(kId3, 150, 0, true));
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      150, 150, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(3);
  VerifyPublicRulesetIDs(*extension(), {kId3});
  scoped_refptr<const Extension> third_extension = extension();
  ASSERT_TRUE(third_extension.get());

  // Combined, the second and third extensions should have 151 rules count
  // towards the global pool.
  EXPECT_EQ(151u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // Check that the prefs entry (or lack thereof) for extra static rule count is
  // correct for each extension.
  CheckExtensionAllocationInPrefs(first_extension.get()->id(), std::nullopt);
  CheckExtensionAllocationInPrefs(second_extension.get()->id(), 101);
  CheckExtensionAllocationInPrefs(third_extension.get()->id(), 50);
}

// Ensure that the global rules limit is enforced correctly for multiple
// extensions.
TEST_P(MultipleRulesetsTest, MultipleExtensionsRuleLimitExceeded) {
  // Load an extension with 300 rules, which reaches the global rules limit.
  AddRuleset(CreateRuleset(kId1, 300, 0, true));
  RulesetManagerObserver ruleset_waiter(manager());

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      300, 300, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  scoped_refptr<const Extension> first_extension = extension();
  ASSERT_TRUE(first_extension.get());
  ExtensionId first_extension_id = first_extension.get()->id();

  VerifyPublicRulesetIDs(*first_extension.get(), {kId1});
  CheckExtensionAllocationInPrefs(first_extension_id, 200);

  // Load a second extension. Only one of its rulesets should be loaded.
  UpdateExtensionLoaderAndPath(
      temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension_2")));
  ClearRulesets();

  AddRuleset(
      CreateRuleset(kId2, GetStaticGuaranteedMinimumRuleCount(), 0, true));
  AddRuleset(CreateRuleset(kId3, 1, 0, true));
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      GetStaticGuaranteedMinimumRuleCount() + 1,
      GetStaticGuaranteedMinimumRuleCount() + 1,
      true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(2);
  scoped_refptr<const Extension> second_extension = extension();
  ASSERT_TRUE(second_extension.get());
  ExtensionId second_extension_id = second_extension.get()->id();

  // Only |kId2| should be enabled as |kId3| causes the global rule limit to be
  // exceeded.
  VerifyPublicRulesetIDs(*second_extension.get(), {kId2});
  CheckExtensionAllocationInPrefs(second_extension_id, std::nullopt);

  // Since the ID of the second extension is known only after it was installed,
  // disable then enable the extension so the ID can be used for the
  // WarningServiceObserver.
  service()->DisableExtension(second_extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  WarningService* warning_service = WarningService::Get(browser_context());
  WarningServiceObserver warning_observer(warning_service, second_extension_id);
  service()->EnableExtension(second_extension_id);

  // Wait until we surface a warning.
  warning_observer.WaitForWarning();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(2);

  // Ensure that a warning was raised for the second extension.
  EXPECT_THAT(
      warning_service->GetWarningTypesAffectingExtension(second_extension_id),
      ::testing::ElementsAre(Warning::kEnabledRuleCountExceeded));

  service()->UninstallExtension(first_extension_id,
                                UNINSTALL_REASON_FOR_TESTING, nullptr);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  service()->DisableExtension(second_extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);
  CheckExtensionAllocationInPrefs(first_extension_id, std::nullopt);
  CheckExtensionAllocationInPrefs(second_extension_id, std::nullopt);

  service()->EnableExtension(second_extension_id);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // Once the first extension is uninstalled, both |kId2| and |kId3| should be
  // enabled.
  VerifyPublicRulesetIDs(*second_extension.get(), {kId2, kId3});
  CheckExtensionAllocationInPrefs(second_extension_id, 1);
  EXPECT_TRUE(
      warning_service->GetWarningTypesAffectingExtension(second_extension_id)
          .empty());
}

TEST_P(MultipleRulesetsTest, UpdateAndGetEnabledRulesets_RuleCountAllocation) {
  AddRuleset(CreateRuleset(kId1, 90, 0, false));
  AddRuleset(CreateRuleset(kId2, 60, 0, true));
  AddRuleset(CreateRuleset(kId3, 150, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      300, 210, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension()->id());
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  CheckExtensionAllocationInPrefs(extension()->id(), 110);

  // Disable |kId2|.
  RunUpdateEnabledRulesetsFunction(*extension(), {kId2}, {},
                                   std::nullopt /* expected_error */);

  VerifyPublicRulesetIDs(*extension(), {kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId3});

  // After |kId2| is disabled, 50 rules should contribute to the global pool.
  GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(50u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // Check that the extra static rule count is also persisted in prefs.
  CheckExtensionAllocationInPrefs(extension()->id(), 50);

  // Enable |kId1|.
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId1},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId1, kId3});

  // After |kId1| is enabled, 140 rules should contribute to the global pool.
  EXPECT_EQ(140u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(extension()->id(), 140);

  // Disable |kId3|.
  RunUpdateEnabledRulesetsFunction(*extension(), {kId3}, {},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId1});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId1});

  // After |kId3| is disabled, no rules should contribute to the global pool and
  // there should not be an entry for the extension in prefs.
  EXPECT_EQ(0u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(extension()->id(), std::nullopt);
}

TEST_P(MultipleRulesetsTest, UpdateAndGetEnabledRulesets_RuleCountExceeded) {
  AddRuleset(CreateRuleset(kId1, 250, 0, true));
  AddRuleset(CreateRuleset(kId2, 40, 0, true));
  AddRuleset(CreateRuleset(kId3, 50, 0, false));

  RulesetManagerObserver ruleset_waiter(manager());

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      340, 290, true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension()->id());
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});
  CheckExtensionAllocationInPrefs(extension()->id(), 190);

  // Disable |kId2| and enable |kId3|.
  RunUpdateEnabledRulesetsFunction(*extension(), {kId2}, {kId3},
                                   std::nullopt /* expected_error */);

  // updateEnabledRulesets looks at the rule counts at the end of the update, so
  // disabling |kId2| and enabling |kId3| works (because the total rule count is
  // under the limit).
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId1, kId3});
  CheckExtensionAllocationInPrefs(extension()->id(), 200);

  // Enable |kId2|. This should not succeed because the global rule limit would
  // be exceeded.
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId2},
                                   kEnabledRulesetsRuleCountExceeded);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId1, kId3});
  CheckExtensionAllocationInPrefs(extension()->id(), 200);
}

TEST_P(MultipleRulesetsTest,
       UpdateAndGetEnabledRulesets_KeepEnabledStaticRulesetsAfterReload) {
  AddRuleset(CreateRuleset(kId1, 90, 0, false));
  AddRuleset(CreateRuleset(kId2, 60, 0, false));
  AddRuleset(CreateRuleset(kId3, 150, 0, false));

  RulesetManagerObserver ruleset_waiter(manager());

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      300, 0, false /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId2, kId3},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Ensure the set of enabled rulesets persists across extension reloads.
  // Regression test for crbug.com/1346185.
  const ExtensionId extension_id = extension()->id();
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  service()->EnableExtension(extension_id);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  const Extension* extension =
      registry()->enabled_extensions().GetByID(extension_id);
  ASSERT_TRUE(extension);
  VerifyPublicRulesetIDs(*extension, {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension, {kId2, kId3});
}

// Tests attempting to disable rulesets when there are no rulesets active.
// Regression test for https://crbug.com/1354385.
TEST_P(MultipleRulesetsTest,
       UpdateAndGetEnabledRulesets_DisableRulesetsWhenEmptyEnabledRulesets) {
  AddRuleset(CreateRuleset(kId1, 90, 0, false));
  AddRuleset(CreateRuleset(kId2, 60, 0, false));
  AddRuleset(CreateRuleset(kId3, 150, 0, false));

  RulesetManagerObserver ruleset_waiter(manager());

  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      300, 0, false /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  // Even though rulesets kId2 and kId3 are already disabled, the service
  // can't know about that right away because there could be pending calls to
  // complete. This means the service will still (appropriately) try and
  // disable these rulesets.
  RunUpdateEnabledRulesetsFunction(*extension(), {kId2, kId3}, {},
                                   std::nullopt /* expected_error */);
  ASSERT_FALSE(manager()->GetMatcherForExtension(extension()->id()));
  VerifyGetEnabledRulesetsFunction(*extension(), {});
}

// Test that getAvailableStaticRuleCount returns the correct number of rules an
// extension can still enable.
TEST_P(MultipleRulesetsTest, GetAvailableStaticRuleCount) {
  AddRuleset(CreateRuleset(kId1, 50, 0, true));
  AddRuleset(CreateRuleset(kId2, 100, 0, false));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(150, 50);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  scoped_refptr<const Extension> first_extension = extension();
  ASSERT_TRUE(first_extension.get());
  ExtensionId first_extension_id = first_extension.get()->id();

  // Initially, the extension should have 250 more static rules available, and
  // no rules allocated from the global pool.
  VerifyPublicRulesetIDs(*first_extension.get(), {kId1});
  CheckExtensionAllocationInPrefs(first_extension_id, std::nullopt);
  VerifyGetAvailableStaticRuleCountFunction(*first_extension.get(), 250);

  // Enabling |kId2| should result in 50 rules allocated in the global pool, and
  // 150 more rules available for the extension to enable.
  RunUpdateEnabledRulesetsFunction(*first_extension.get(), {}, {kId2},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*first_extension.get(), {kId1, kId2});
  CheckExtensionAllocationInPrefs(first_extension_id, 50);
  VerifyGetAvailableStaticRuleCountFunction(*first_extension.get(), 150);

  // Disabling all rulesets should result in 300 rules available.
  RunUpdateEnabledRulesetsFunction(*first_extension.get(), {kId1, kId2}, {},
                                   std::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*first_extension.get(), {});
  CheckExtensionAllocationInPrefs(first_extension_id, std::nullopt);
  VerifyGetAvailableStaticRuleCountFunction(*first_extension.get(), 300);

  // Load another extension with one ruleset with 300 rules.
  UpdateExtensionLoaderAndPath(
      temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension_2")));
  ClearRulesets();

  AddRuleset(CreateRuleset(kId3, GetMaximumRulesPerRuleset(), 0, true));
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      GetMaximumRulesPerRuleset(), GetMaximumRulesPerRuleset(),
      true /* expect_rulesets_indexed */);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(2);
  scoped_refptr<const Extension> second_extension = extension();
  ASSERT_TRUE(second_extension.get());
  ExtensionId second_extension_id = second_extension.get()->id();

  VerifyPublicRulesetIDs(*second_extension.get(), {kId3});
  CheckExtensionAllocationInPrefs(second_extension_id, 200);

  // The first extension should still have GetStaticGuaranteedMinimumRuleCount()
  // rules available as it has no rules enabled and the global pool is full.
  VerifyGetAvailableStaticRuleCountFunction(
      *first_extension.get(), GetStaticGuaranteedMinimumRuleCount());

  // The second extension should not have any rules available since its
  // allocation consists of the entire global pool.
  VerifyGetAvailableStaticRuleCountFunction(*second_extension.get(), 0);
}

// Test to update disabled rule ids of static rulesets.
TEST_P(MultipleRulesetsTest, UpdateStaticRulesDisableAndEnableRules) {
  AddRuleset(CreateRuleset(kId1, 5, 0, true));
  AddRuleset(CreateRuleset(kId2, 5, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(10, 10);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});

  // The initial disabled rule ids set is empty.
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2), testing::IsEmpty());
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {});
  EXPECT_EQ(0u, GetDisabledStaticRuleCount());

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  RunUpdateStaticRulesFunction(*extension(), kId1, {1, 2, 3}, {},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2), testing::IsEmpty());
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {});
  EXPECT_EQ(3u, GetDisabledStaticRuleCount());

  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  RunUpdateStaticRulesFunction(*extension(), kId2, {3, 4, 5}, {},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 4, 5});
  EXPECT_EQ(6u, GetDisabledStaticRuleCount());

  // Enable rule 1, rule 2 rule 3 and rule 4 of ruleset1. Enabling rule 4
  // doesn't make any change since rule 4 is not disabled.
  RunUpdateStaticRulesFunction(*extension(), kId1, {}, {1, 2, 3, 4},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 4, 5});
  EXPECT_EQ(3u, GetDisabledStaticRuleCount());

  // Enable rule 3, rule 4, rule 5 and rule 6 of ruleset2. Enabling
  // rule 6 doesn't make any change since rule 6 is not disabled.
  RunUpdateStaticRulesFunction(*extension(), kId2, {}, {3, 4, 5, 6},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1), testing::IsEmpty());
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2), testing::IsEmpty());
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {});
  EXPECT_EQ(0u, GetDisabledStaticRuleCount());
}

// Test UpdateStaticRules making no change.
TEST_P(MultipleRulesetsTest, UpdateStaticRulesMakingNoChange) {
  AddRuleset(CreateRuleset(kId1, 5, 0, true));
  AddRuleset(CreateRuleset(kId2, 5, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(10, 10);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  RunUpdateStaticRulesFunction(*extension(), kId1, {1, 2, 3}, {},
                               std::nullopt /* expected_error */);
  RunUpdateStaticRulesFunction(*extension(), kId2, {3, 4, 5}, {},
                               std::nullopt /* expected_error */);

  // Updating disabled rule ids with null set doesn't make any change.
  RunUpdateStaticRulesFunction(*extension(), kId2, {}, {},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 4, 5});
  EXPECT_EQ(6u, GetDisabledStaticRuleCount());

  // Fails to enable rule 8, rule 9 and rule 10 of ruleset3 since ruleset3 is
  // invalid ruleset id.
  RunUpdateStaticRulesFunction(
      *extension(), kId3, {3, 4, 5}, {},
      ErrorUtils::FormatErrorMessage(kInvalidRulesetIDError, kId3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 4, 5});
  EXPECT_FALSE(RulesetExists(kId3));
  EXPECT_EQ(6u, GetDisabledStaticRuleCount());
}

// Test to check UpdateStaticRules argument priority.
TEST_P(MultipleRulesetsTest, UpdateStaticRulesArgumentPriority) {
  AddRuleset(CreateRuleset(kId1, 5, 0, true));
  AddRuleset(CreateRuleset(kId2, 5, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(10, 10);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  RunUpdateStaticRulesFunction(*extension(), kId1, {1, 2, 3}, {},
                               std::nullopt /* expected_error */);
  RunUpdateStaticRulesFunction(*extension(), kId2, {3, 4, 5}, {},
                               std::nullopt /* expected_error */);

  // Disable rule 4 and rule 5 of ruleset2 but it doesn't make any change since
  // they are already disabled. Ignore enabling rule 5 since |ids_to_disable|
  // takes priority over |ids_to_enable|.
  RunUpdateStaticRulesFunction(*extension(), kId2, {4, 5}, {5},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 4, 5});
  EXPECT_EQ(6u, GetDisabledStaticRuleCount());

  // Enable rule 4 and disable rule 5, rule 6 and rule 7 of ruleset2. Ignore
  // enabling rule 5 since |ids_to_disable| takes priority over |ids_to_enable|.
  // Disabling rule 5 doesn't make any change since rule 5 is already disabled.
  RunUpdateStaticRulesFunction(*extension(), kId2, {5, 6, 7}, {4, 5},
                               std::nullopt /* expected_error */);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 5, 6, 7));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 5, 6, 7});
  EXPECT_EQ(7u, GetDisabledStaticRuleCount());
}

// Test to check UpdateStaticRules error when rule limit exceeded.
TEST_P(MultipleRulesetsTest, UpdateStaticRulesErrorWhenRuleLimitExceeded) {
  // Set the disabled static rule limit as 6.
  ScopedRuleLimitOverride scoped_disabled_static_rule_limit_override =
      CreateScopedDisabledStaticRuleLimitOverrideForTesting(6);

  AddRuleset(CreateRuleset(kId1, 5, 0, true));
  AddRuleset(CreateRuleset(kId2, 5, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(10, 10);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  RunUpdateStaticRulesFunction(*extension(), kId1, {1, 2, 3}, {},
                               std::nullopt /* expected_error */);
  RunUpdateStaticRulesFunction(*extension(), kId2, {3, 4, 5}, {},
                               std::nullopt /* expected_error */);

  // Enable rule 1 and disable rule 3, rule 4 and rule 5 of ruleset2. Ignore
  // enabling rule 3 since |ids_to_disable| takes priority over |ids_to_enable|.
  // This operation fails since it exceeds the disabled static rule count limit.
  RunUpdateStaticRulesFunction(*extension(), kId1, {3, 4, 5}, {1, 3},
                               kDisabledStaticRuleCountExceeded);
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension(), kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension(), kId2, {3, 4, 5});
  EXPECT_EQ(6u, GetDisabledStaticRuleCount());
}

// Test to check GetDisabledRuleIds error for invalid ruleset id.
TEST_P(MultipleRulesetsTest, GetDisabledStaticRuleIdsErrorForInvalidRuleset) {
  AddRuleset(CreateRuleset(kId1, 5, 0, true));
  AddRuleset(CreateRuleset(kId2, 5, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(10, 10);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  RunUpdateStaticRulesFunction(*extension(), kId1, {1, 2, 3}, {},
                               std::nullopt /* expected_error */);
  RunUpdateStaticRulesFunction(*extension(), kId2, {3, 4, 5}, {},
                               std::nullopt /* expected_error */);

  VerifyGetDisabledRuleIdsFunctionError(
      *extension(), kId3,
      ErrorUtils::FormatErrorMessage(kInvalidRulesetIDError, kId3));
}

// Test the disabled rule ids when the extension is disabled and enabled.
TEST_P(MultipleRulesetsTest,
       KeepDisabledStaticRulesWhenExtensionDisabledAndEnabled) {
  AddRuleset(CreateRuleset(kId1, 5, 0, true));
  AddRuleset(CreateRuleset(kId2, 5, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());

  LoadAndExpectSuccess(10, 10);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  VerifyPublicRulesetIDs(*extension(), {kId1, kId2});

  // Disable rule 1, rule 2 and rule 3 of ruleset1.
  // Disable rule 3, rule 4 and rule 5 of ruleset2.
  RunUpdateStaticRulesFunction(*extension(), kId1, {1, 2, 3}, {},
                               std::nullopt /* expected_error */);
  RunUpdateStaticRulesFunction(*extension(), kId2, {3, 4, 5}, {},
                               std::nullopt /* expected_error */);

  // Check disabled rules after disabling and enabling extension.
  auto extension_id = extension()->id();
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);
  TestExtensionRegistryObserver registry_observer(registry());
  service()->EnableExtension(extension_id);
  scoped_refptr<const Extension> extension =
      registry_observer.WaitForExtensionLoaded();
  ASSERT_TRUE(extension);
  ASSERT_EQ(extension_id, extension->id());
  content::RunAllTasksUntilIdle();

  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId1),
              UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(GetDisabledRuleIdsFromMatcher(kId2),
              UnorderedElementsAre(3, 4, 5));
  VerifyGetDisabledRuleIdsFunction(*extension, kId1, {1, 2, 3});
  VerifyGetDisabledRuleIdsFunction(*extension, kId2, {3, 4, 5});
  EXPECT_EQ(6u, GetDisabledStaticRuleCount());
}

// Test that an extension's allocation is reclaimed when unloaded in certain
// scenarios.
TEST_P(MultipleRulesetsTest, ReclaimAllocationOnUnload) {
  const size_t ext_1_allocation = 50;

  AddRuleset(CreateRuleset(
      kId1, GetStaticGuaranteedMinimumRuleCount() + ext_1_allocation, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess(GetStaticGuaranteedMinimumRuleCount() +
                       ext_1_allocation);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  ExtensionId first_extension_id = extension()->id();

  // The |ext_1_allocation| rules that contribute to the global pool should be
  // tracked.
  GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(ext_1_allocation,
            global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // An entry for these |ext_1_allocation| rules should be persisted for the
  // extension in prefs.
  CheckExtensionAllocationInPrefs(first_extension_id, ext_1_allocation);

  auto disable_extension_and_check_allocation =
      [this, &ext_1_allocation, &global_rules_tracker, &ruleset_waiter,
       &first_extension_id](int disable_reasons,
                            bool expect_allocation_released) {
        service()->DisableExtension(first_extension_id, disable_reasons);
        ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

        size_t expected_tracker_allocation =
            expect_allocation_released ? 0 : ext_1_allocation;
        std::optional<size_t> expected_pref_allocation =
            expect_allocation_released
                ? std::nullopt
                : std::make_optional<size_t>(ext_1_allocation);
        EXPECT_EQ(expected_tracker_allocation,
                  global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
        CheckExtensionAllocationInPrefs(first_extension_id,
                                        expected_pref_allocation);

        service()->EnableExtension(first_extension_id);
        ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

        EXPECT_EQ(ext_1_allocation,
                  global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
        CheckExtensionAllocationInPrefs(first_extension_id, ext_1_allocation);
      };

  // Test some DisableReasons that shouldn't cause the allocation to be
  // released.
  disable_extension_and_check_allocation(
      disable_reason::DISABLE_PERMISSIONS_INCREASE |
          disable_reason::DISABLE_GREYLIST,
      false);

  // Test the DisableReasons that should cause the allocation to be released.
  disable_extension_and_check_allocation(disable_reason::DISABLE_USER_ACTION,
                                         true);

  disable_extension_and_check_allocation(
      disable_reason::DISABLE_BLOCKED_BY_POLICY, true);

  disable_extension_and_check_allocation(
      disable_reason::DISABLE_BLOCKED_BY_POLICY |
          disable_reason::DISABLE_GREYLIST,
      true);

  // We should reclaim the extension's allocation if it is blocklisted.
  service()->BlocklistExtensionForTest(first_extension_id);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);
  EXPECT_EQ(0u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(first_extension_id, std::nullopt);

  // Load another extension, only to have it be terminated.
  const size_t ext_2_allocation = 50;
  UpdateExtensionLoaderAndPath(
      temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension_2")));
  ClearRulesets();

  AddRuleset(CreateRuleset(
      kId2, GetStaticGuaranteedMinimumRuleCount() + ext_2_allocation, 0, true));
  LoadAndExpectSuccess(GetStaticGuaranteedMinimumRuleCount() +
                       ext_2_allocation);

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  ExtensionId second_extension_id = extension()->id();

  // The extension should have its allocation kept when it is terminated.
  service()->TerminateExtension(second_extension_id);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);
  EXPECT_EQ(ext_2_allocation,
            global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(second_extension_id, ext_2_allocation);
}

using MultipleRulesetsTest_Unpacked = MultipleRulesetsTest;

// Test that reloading an unpacked extension is functionally identical to
// uninstalling then reinstalling it for the purpose of global rule allocation,
// and the allocation should reflect changes made to the extension.
TEST_P(MultipleRulesetsTest_Unpacked, UpdateAllocationOnReload) {
  AddRuleset(CreateRuleset(kId1, 250, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess(250);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  ExtensionId extension_id = extension()->id();

  // The 150 rules that contribute to the global pool should be
  // tracked.
  GlobalRulesTracker& global_rules_tracker =
      RulesMonitorService::Get(browser_context())->global_rules_tracker();
  EXPECT_EQ(150u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());

  // An entry for these 150 rules should be persisted for the extension in
  // prefs.
  CheckExtensionAllocationInPrefs(extension_id, 150);

  // Replace ruleset |kId1| with a smaller ruleset |kId2| and persist the
  // ruleset to the extension's directory via WriteExtensionData().
  ClearRulesets();
  AddRuleset(CreateRuleset(kId2, 150, 0, true));
  WriteExtensionData();

  // Reload the extension. For unpacked extensions this is functionally
  // equivalent to uninstalling the extension then installing it again based on
  // the contents of the extension's directory.
  service()->ReloadExtension(extension_id);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  // File changes to the extension's ruleset should take effect after it is
  // reloaded.
  EXPECT_EQ(50u, global_rules_tracker.GetAllocatedGlobalRuleCountForTesting());
  CheckExtensionAllocationInPrefs(extension_id, 50);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleRulesetTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(All,
                         SingleRulesetWithoutSafeRulesTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(All,
                         MultipleRulesetsTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

INSTANTIATE_TEST_SUITE_P(All,
                         MultipleRulesetsTest_Unpacked,
                         ::testing::Values(ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
