// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
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
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/rules_monitor_service.h"
#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/value_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";

constexpr char kLargeRegexFilter[] = ".{512}x";

constexpr char kId1[] = "1.json";
constexpr char kId2[] = "2.json";
constexpr char kId3[] = "3.json";
constexpr char kId4[] = "4.json";
constexpr char kDefaultRulesetID[] = "id";

namespace dnr_api = extensions::api::declarative_net_request;

using ::testing::Field;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

std::string GetParseError(ParseResult result, int rule_id) {
  return ParseInfo(result, &rule_id).error();
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

    loader_ = CreateExtensionLoader();
    extension_dir_ =
        temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension"));

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir_));
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

    EXPECT_TRUE(
        AreAllIndexedStaticRulesetsValid(*extension_, browser_context()));

    // Ensure no load errors were reported.
    EXPECT_TRUE(error_reporter()->GetErrors()->empty());

    // The histograms below are not logged for unpacked extensions.
    if (GetParam() == ExtensionLoadType::PACKED) {
      int expected_samples = expect_rulesets_indexed ? 1 : 0;

      tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                              expected_samples);
      tester.ExpectUniqueSample(kManifestRulesCountHistogram,
                                expected_rules_count, expected_samples);
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
    const std::vector<base::string16>* errors = error_reporter()->GetErrors();
    ASSERT_EQ(1u, errors->size());
    EXPECT_NE(base::string16::npos,
              errors->at(0).find(base::UTF8ToUTF16(error_with_filename)))
        << "expected: " << error_with_filename << " actual: " << errors->at(0);

    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kManifestRulesCountHistogram, 0u);
  }

  bool RunDynamicRuleUpdateFunction(const Extension& extension,
                                    const std::vector<int>& rule_ids_to_remove,
                                    const std::vector<TestRule>& rules_to_add) {
    std::unique_ptr<base::Value> ids_to_remove_value =
        ListBuilder()
            .Append(rule_ids_to_remove.begin(), rule_ids_to_remove.end())
            .Build();
    std::unique_ptr<base::Value> rules_to_add_value = ToListValue(rules_to_add);

    constexpr const char kParams[] = R"(
      [{
        "addRules": $1,
        "removeRuleIds": $2
      }]
    )";
    const std::string json_args =
        content::JsReplace(kParams, std::move(*rules_to_add_value),
                           std::move(*ids_to_remove_value));

    auto update_function =
        base::MakeRefCounted<DeclarativeNetRequestUpdateDynamicRulesFunction>();
    update_function->set_extension(&extension);
    update_function->set_has_callback(true);
    return api_test_utils::RunFunction(update_function.get(), json_args,
                                       browser_context());
  }

  void RunUpdateEnabledRulesetsFunction(
      const Extension& extension,
      const std::vector<std::string>& ruleset_ids_to_remove,
      const std::vector<std::string>& ruleset_ids_to_add,
      base::Optional<std::string> expected_error) {
    std::unique_ptr<base::Value> ids_to_remove_value =
        ToListValue(ruleset_ids_to_remove);
    std::unique_ptr<base::Value> ids_to_add_value =
        ToListValue(ruleset_ids_to_add);

    constexpr const char kParams[] = R"(
      [{
        "disableRulesetIds": $1,
        "enableRulesetIds": $2
      }]
    )";
    const std::string json_args = content::JsReplace(
        kParams, std::move(*ids_to_remove_value), std::move(*ids_to_add_value));

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

    std::unique_ptr<base::Value> result =
        api_test_utils::RunFunctionAndReturnSingleResult(
            function.get(), "[]" /* args */, browser_context());
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_list());
    const base::ListValue& ids_value = base::Value::AsListValue(*result);

    base::string16 error;
    std::vector<std::string> actual_ids;
    for (const auto& val : ids_value)
      actual_ids.push_back(val.GetString());

    EXPECT_THAT(expected_ids, UnorderedElementsAreArray(actual_ids));
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
  void SetRules(std::unique_ptr<base::Value> rules) {
    rules_value_ = std::move(rules);
  }

  void set_persist_invalid_json_file() { persist_invalid_json_file_ = true; }

  void set_persist_initial_indexed_ruleset() {
    persist_initial_indexed_ruleset_ = true;
  }

  void LoadAndExpectError(const std::string& expected_error) {
    DeclarativeNetRequestUnittest::LoadAndExpectError(expected_error,
                                                      kJSONRulesFilename);
  }

  // |expected_rules_count| refers to the count of indexed rules. When
  // |expected_rules_count| is not set, it is inferred from the added rules.
  void LoadAndExpectSuccess(
      const base::Optional<size_t>& expected_rules_count = base::nullopt) {
    size_t rules_count = 0;
    if (expected_rules_count)
      rules_count = *expected_rules_count;
    else if (rules_value_ && rules_value_->is_list())
      rules_count = rules_value_->GetList().size();
    else
      rules_count = rules_list_.size();

    // We only index up to GetStaticRuleLimit() rules per ruleset.
    rules_count =
        std::min(rules_count, static_cast<size_t>(GetStaticRuleLimit()));

    DeclarativeNetRequestUnittest::LoadAndExpectSuccess(rules_count,
                                                        rules_count, true);
  }

 private:
  // DeclarativeNetRequestUnittest override:
  void WriteExtensionData() override {
    if (!rules_value_)
      rules_value_ = ToListValue(rules_list_);

    WriteManifestAndRuleset(
        extension_dir(),
        TestRulesetInfo(kDefaultRulesetID, kJSONRulesFilename, *rules_value_),
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
  std::unique_ptr<base::Value> rules_value_;
  bool persist_invalid_json_file_ = false;
  bool persist_initial_indexed_ruleset_ = false;
};

TEST_P(SingleRulesetTest, DuplicateResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types =
      std::vector<std::string>({"image", "stylesheet"});
  rule.condition->excluded_resource_types = std::vector<std::string>({"image"});
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED, *rule.id));
}

TEST_P(SingleRulesetTest, EmptyRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("https://google.com");
  rule.priority.reset();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_RULE_PRIORITY, *rule.id));
}

TEST_P(SingleRulesetTest, EmptyRedirectRuleUrl) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  AddRule(rule);

  rule.id = kMinValidID + 1;
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  AddRule(rule);

  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_REDIRECT, *rule.id));
}

TEST_P(SingleRulesetTest, InvalidRuleID) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID - 1;
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_RULE_ID, *rule.id));
}

TEST_P(SingleRulesetTest, InvalidRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("https://google.com");
  rule.priority = kMinValidPriority - 1;
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_RULE_PRIORITY, *rule.id));
}

TEST_P(SingleRulesetTest, NoApplicableResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_resource_types = std::vector<std::string>(
      {"main_frame", "sub_frame", "stylesheet", "script", "image", "font",
       "object", "xmlhttprequest", "ping", "csp_report", "media", "websocket",
       "other"});
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES, *rule.id));
}

TEST_P(SingleRulesetTest, EmptyDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_DOMAINS_LIST, *rule.id));
}

TEST_P(SingleRulesetTest, EmptyResourceTypeList) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST, *rule.id));
}

TEST_P(SingleRulesetTest, EmptyURLFilter) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string();
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_EMPTY_URL_FILTER, *rule.id));
}

TEST_P(SingleRulesetTest, InvalidRedirectURL) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("google");
  rule.priority = kMinValidPriority;
  AddRule(rule);
  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_REDIRECT_URL, *rule.id));
}

TEST_P(SingleRulesetTest, ListNotPassed) {
  SetRules(std::make_unique<base::DictionaryValue>());
  LoadAndExpectError(kErrorListNotPassed);
}

TEST_P(SingleRulesetTest, DuplicateIDS) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);
  AddRule(rule);
  LoadAndExpectError(GetParseError(ParseResult::ERROR_DUPLICATE_IDS, *rule.id));
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

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    const std::vector<InstallWarning>& expected_warnings =
        extension()->install_warnings();
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
        std::to_string(kMaxUnparsedRulesWarnings));
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

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(2u, extension()->install_warnings().size());
    std::vector<InstallWarning> expected_warnings;

    for (const auto& warning : extension()->install_warnings()) {
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
  SetRules(base::JSONReader::ReadDeprecated(kRules));

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(1u);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(3u, extension()->install_warnings().size());
    std::vector<InstallWarning> expected_warnings;

    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "id 1",
            "'condition': expected dictionary, got list"),
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);
    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "id 3",
            "found unexpected key 'invalidKey'"),
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);
    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(
            GetErrorWithFilename(kRuleNotParsedWarning), "index 4",
            "'id': expected id, got string"),
        dnr_api::ManifestKeys::kDeclarativeNetRequest,
        dnr_api::DNRInfo::kRuleResources);
    EXPECT_EQ(expected_warnings, extension()->install_warnings());
  }
}

// Ensure that we can add up to GetStaticRuleLimit() rules.
TEST_P(SingleRulesetTest, RuleCountLimitMatched) {
  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);

  TestRule rule = CreateGenericRule();
  for (int i = 0; i < GetStaticRuleLimit(); ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }
  LoadAndExpectSuccess();
}

// Ensure that we get an install warning on exceeding the rule count limit.
TEST_P(SingleRulesetTest, RuleCountLimitExceeded) {
  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);

  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= GetStaticRuleLimit() + 1; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess();

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(1u, extension()->install_warnings().size());
    EXPECT_EQ(InstallWarning(GetErrorWithFilename(kRuleCountExceeded),
                             dnr_api::ManifestKeys::kDeclarativeNetRequest,
                             dnr_api::DNRInfo::kRuleResources),
              extension()->install_warnings()[0]);
  }
}

// Ensure that we can add up to the |dnr_api::GUARANTEED_MINIMUM_STATIC_RULES| +
// |kMaxStaticRulesPerProfile| rules if the global rules feature is enabled.
TEST_P(SingleRulesetTest, RuleCountLimitMatched_GlobalRules) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kDeclarativeNetRequestGlobalRules);

  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);

  // Similarly, override the global limit to prevent a timeout.
  base::AutoReset<int> global_limit_override =
      CreateScopedGlobalStaticRuleLimitOverrideForTesting(200);

  // Sanity check that the extension can index and enable up to
  // |rule_limit_override| + |global_limit_override| rules.
  ASSERT_EQ(300, GetStaticRuleLimit());

  TestRule rule = CreateGenericRule();
  for (int i = 0; i < GetStaticRuleLimit(); ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(300);

  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension());

  ASSERT_EQ(1u, static_sources.size());
  EXPECT_TRUE(base::PathExists(static_sources[0].indexed_path()));

  // The ruleset's ID should not be marked as ignored in prefs.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  EXPECT_FALSE(
      prefs->ShouldIgnoreDNRRuleset(extension()->id(), static_sources[0].id()));
}

// Ensure that we get an install warning on exceeding the rule count limit and
// that no rules are indexed.
TEST_P(SingleRulesetTest, RuleCountLimitExceeded_GlobalRules) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kDeclarativeNetRequestGlobalRules);

  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);

  // Similarly, override the global limit to prevent a timeout.
  base::AutoReset<int> global_limit_override =
      CreateScopedGlobalStaticRuleLimitOverrideForTesting(200);

  ASSERT_EQ(300, GetStaticRuleLimit());

  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= GetStaticRuleLimit() + 1; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  DeclarativeNetRequestUnittest::LoadAndExpectSuccess(
      0, 0, false /* expect_rulesets_indexed */);

  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension());

  // Since the ruleset was ignored and not indexed, it should not be persisted
  // to a file.
  ASSERT_EQ(1u, static_sources.size());
  EXPECT_FALSE(base::PathExists(static_sources[0].indexed_path()));

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(1u, extension()->install_warnings().size());
    InstallWarning expected_warning =
        InstallWarning(GetErrorWithFilename(ErrorUtils::FormatErrorMessage(
                           kIndexingRuleLimitExceeded,
                           std::to_string(static_sources[0].id().value()))),
                       dnr_api::ManifestKeys::kDeclarativeNetRequest,
                       dnr_api::DNRInfo::kRuleResources);

    EXPECT_EQ(expected_warning, extension()->install_warnings()[0]);
  }

  // The ruleset's ID should be persisted in the ignored rulesets pref.
  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());
  EXPECT_TRUE(
      prefs->ShouldIgnoreDNRRuleset(extension()->id(), static_sources[0].id()));
}

// Ensure that regex rules which exceed the per rule memory limit are ignored
// and raise an install warning.
TEST_P(SingleRulesetTest, LargeRegexIgnored) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter.reset();
  int id = kMinValidID;

  const int kNumSmallRegex = 5;
  std::string small_regex = "http://(yahoo|google)\\.com";
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

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    InstallWarning warning_1 = GetLargeRegexWarning(kMinValidID + 5);
    InstallWarning warning_2 = GetLargeRegexWarning(kMinValidID + 6);
    EXPECT_THAT(extension()->install_warnings(),
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

  LoadAndExpectError(
      GetParseError(ParseResult::ERROR_INVALID_REGEX_FILTER, kMinValidID + 1));
}

// Ensure that we get an install warning on exceeding the regex rule count
// limit.
TEST_P(SingleRulesetTest, RegexRuleCountExceeded) {
  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedRegexRuleLimitOverrideForTesting(100);

  TestRule regex_rule = CreateGenericRule();
  regex_rule.condition->url_filter.reset();
  int rule_id = kMinValidID;
  for (int i = 1; i <= GetRegexRuleLimit() + 5; ++i, ++rule_id) {
    regex_rule.id = rule_id;
    regex_rule.condition->regex_filter = std::to_string(i);
    AddRule(regex_rule);
  }

  const int kCountNonRegexRules = 5;
  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= kCountNonRegexRules; i++, ++rule_id) {
    rule.id = rule_id;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(GetRegexRuleLimit() + kCountNonRegexRules);
  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(1u, extension()->install_warnings().size());
    EXPECT_EQ(InstallWarning(GetErrorWithFilename(kRegexRuleCountExceeded),
                             dnr_api::ManifestKeys::kDeclarativeNetRequest,
                             dnr_api::DNRInfo::kRuleResources),
              extension()->install_warnings()[0]);
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
  ASSERT_TRUE(RunDynamicRuleUpdateFunction(
      *extension, {} /* rule_ids_to_remove */, dynamic_rules));

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
                                   base::nullopt);

  // Wait for any pending tasks. This isn't actually necessary for this test
  // (there shouldn't be any pending tasks at this point). However still do this
  // to not rely on any task ordering assumption.
  content::RunAllTasksUntilIdle();

  // The API function to update the enabled rulesets should only complete after
  // the initial ruleset loading (in response to OnExtensionLoaded) is complete.
  // Hence by now, the extension shouldn't have any active rulesets.
  VerifyPublicRulesetIDs(*extension, {});
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

    TestRule regex_rule = CreateGenericRule();
    regex_rule.condition->url_filter.reset();
    regex_rule.condition->regex_filter = "block";
    for (size_t i = 0; i < num_regex_rules; ++i, ++id) {
      regex_rule.id = id;
      rules.push_back(regex_rule);
    }

    return TestRulesetInfo(manifest_id_and_path, *ToListValue(rules), enabled);
  }

  // |expected_rules_count| and |expected_enabled_rules_count| refer to the
  // counts of indexed rules. When not set, these are inferred from the added
  // rulesets.
  void LoadAndExpectSuccess(
      const base::Optional<size_t>& expected_rules_count = base::nullopt,
      const base::Optional<size_t>& expected_enabled_rules_count =
          base::nullopt) {
    size_t rules_count = 0u;
    size_t rules_enabled_count = 0u;
    for (const TestRulesetInfo& info : rulesets_) {
      size_t count = info.rules_value.GetList().size();

      // We only index up to GetStaticRuleLimit() rules per ruleset, but may
      // index more rules than this limit across rulesets.
      count = std::min(count, static_cast<size_t>(GetStaticRuleLimit()));

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
    AddRuleset(CreateRuleset(std::to_string(i), kRulesPerRuleset, 0, true));
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
    AddRuleset(CreateRuleset(std::to_string(i), 0, 0, true));

  LoadAndExpectSuccess();
}

// Tests an extension with multiple static rulesets, with one of rulesets
// specifying an invalid rules file.
TEST_P(MultipleRulesetsTest, ListNotPassed) {
    std::vector<TestRule> rules({CreateGenericRule()});
    AddRuleset(TestRulesetInfo(kId1, "path1", *ToListValue(rules)));

    // Persist a ruleset with an invalid rules file.
    AddRuleset(TestRulesetInfo(kId2, "path2", base::DictionaryValue()));

    AddRuleset(TestRulesetInfo(kId3, "path3", base::ListValue()));

    LoadAndExpectError(kErrorListNotPassed, "path2" /* filename */);
}

// Tests an extension with multiple static rulesets with each ruleset generating
// some install warnings.
TEST_P(MultipleRulesetsTest, InstallWarnings) {
  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);
  base::AutoReset<int> regex_rule_limit_override =
      CreateScopedRegexRuleLimitOverrideForTesting(60);

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

    TestRulesetInfo info(kId1, "path1", *ToListValue(rules), true);
    AddRuleset(info);

    expected_warnings.push_back(
        GetLargeRegexWarning(*rule.id, info.relative_file_path).message);

    expected_rule_count += rules.size();
    enabled_rule_count += 1;
  }

  {
    // Persist a ruleset with an install warning for exceeding the rule count.
    TestRulesetInfo info =
        CreateRuleset(kId2, GetStaticRuleLimit() + 1, 0, false);
    AddRuleset(info);

    expected_warnings.push_back(
        GetErrorWithFilename(kRuleCountExceeded, info.relative_file_path));

    expected_rule_count += GetStaticRuleLimit();
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

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    const std::vector<InstallWarning>& warnings =
        extension()->install_warnings();
    std::vector<std::string> warning_strings;
    for (const InstallWarning& warning : warnings)
      warning_strings.push_back(warning.message);

    EXPECT_THAT(warning_strings, UnorderedElementsAreArray(expected_warnings));
  }
}

TEST_P(MultipleRulesetsTest, EnabledRulesCount) {
  AddRuleset(CreateRuleset(kId1, 100, 10, true));
  AddRuleset(CreateRuleset(kId2, 200, 20, false));
  AddRuleset(CreateRuleset(kId3, 300, 30, true));

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
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 300 + 30))));
}

// Ensure that exceeding the rules count limit across rulesets raises an install
// warning.
TEST_P(MultipleRulesetsTest, StaticRuleCountExceeded) {
  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(50);

  // Enabled on load.
  AddRuleset(CreateRuleset(kId1, 10, 0, true));
  // Disabled by default.
  AddRuleset(CreateRuleset(kId2, 20, 0, false));
  // Not enabled on load since including it exceeds the static rules count.
  AddRuleset(CreateRuleset(kId3, GetStaticRuleLimit() + 10, 0, true));
  // Enabled on load.
  AddRuleset(CreateRuleset(kId4, 30, 0, true));

  RulesetManagerObserver ruleset_waiter(manager());
  extension_loader()->set_ignore_manifest_warnings(true);

  {
    // To prevent timeouts in debug builds, increase the wait timeout to the
    // test launcher's timeout. See crbug.com/1071403.
    base::test::ScopedRunLoopTimeout specific_timeout(
        FROM_HERE, TestTimeouts::test_launcher_timeout());
    LoadAndExpectSuccess();
  }

  std::string extension_id = extension()->id();

  // Installing the extension causes install warning for rulesets 2 and 3 since
  // they exceed the rules limit. Also, since the set of enabled rulesets exceed
  // the rules limit, another warning should be raised.
  if (GetParam() != ExtensionLoadType::PACKED) {
    EXPECT_THAT(
        extension()->install_warnings(),
        UnorderedElementsAre(
            Field(&InstallWarning::message,
                  GetErrorWithFilename(kRuleCountExceeded, kId3)),
            Field(&InstallWarning::message, kEnabledRuleCountExceeded)));
  }

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension_id);
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId1, kId4});

  EXPECT_THAT(composite_matcher->matchers(),
              UnorderedElementsAre(
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 10)),
                  Pointee(Property(&RulesetMatcher::GetRulesCount, 30))));
}

// Ensure that exceeding the regex rules limit across rulesets raises a warning.
TEST_P(MultipleRulesetsTest, RegexRuleCountExceeded) {
  // Enabled on load.
  AddRuleset(CreateRuleset(kId1, 10000, 100, true));
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
    EXPECT_THAT(extension()->install_warnings(),
                UnorderedElementsAre(Field(&InstallWarning::message,
                                           kEnabledRegexRuleCountExceeded)));
  }

  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  CompositeMatcher* composite_matcher =
      manager()->GetMatcherForExtension(extension()->id());
  ASSERT_TRUE(composite_matcher);

  VerifyPublicRulesetIDs(*extension(), {kId1, kId4});

  EXPECT_THAT(
      composite_matcher->matchers(),
      UnorderedElementsAre(
          Pointee(Property(&RulesetMatcher::GetRulesCount, 10000 + 100)),
          Pointee(Property(&RulesetMatcher::GetRulesCount, 20 + 20))));
}

// Ensure that only rulesets which exceed the rules count limit will not have
// their rules indexed and will raise an install warning.
TEST_P(MultipleRulesetsTest, StaticRuleCountExceeded_GlobalRules) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kDeclarativeNetRequestGlobalRules);

  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);

  // Similarly, override the global limit to prevent a timeout.
  base::AutoReset<int> global_limit_override =
      CreateScopedGlobalStaticRuleLimitOverrideForTesting(200);

  ASSERT_EQ(300, GetStaticRuleLimit());

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

  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension());
  ASSERT_EQ(2u, static_sources.size());

  if (GetParam() != ExtensionLoadType::PACKED) {
    std::string expected_warning = GetErrorWithFilename(
        ErrorUtils::FormatErrorMessage(
            kIndexingRuleLimitExceeded,
            std::to_string(static_sources[0].id().value())),
        kId1);

    EXPECT_THAT(extension()->install_warnings(),
                UnorderedElementsAre(
                    Field(&InstallWarning::message, expected_warning)));
  }

  // Since the first ruleset was ignored and not indexed, it should not be
  // persisted to a file.
  EXPECT_FALSE(base::PathExists(static_sources[0].indexed_path()));

  // The second ruleset was indexed and it should be persisted.
  EXPECT_TRUE(base::PathExists(static_sources[1].indexed_path()));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context());

  // The first ruleset's ID should be persisted in the ignored rulesets pref.
  EXPECT_TRUE(
      prefs->ShouldIgnoreDNRRuleset(extension()->id(), static_sources[0].id()));

  // The second ruleset's ID should not be marked as ignored in prefs.
  EXPECT_FALSE(
      prefs->ShouldIgnoreDNRRuleset(extension()->id(), static_sources[1].id()));
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
}

TEST_P(MultipleRulesetsTest, UpdateEnabledRulesets_RuleCountExceeded) {
  // Override the API rule limit to prevent a timeout on loading the extension.
  base::AutoReset<int> rule_limit_override =
      CreateScopedStaticRuleLimitOverrideForTesting(100);

  AddRuleset(CreateRuleset(kId1, 10, 10, true));
  AddRuleset(CreateRuleset(kId2, GetStaticRuleLimit(), 0, false));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId2},
                                   kEnabledRulesetsRuleCountExceeded);
  VerifyPublicRulesetIDs(*extension(), {kId1});

  // updateEnabledRulesets looks at the rule counts at the end of the update, so
  // disabling |kId1| and enabling |kId2| works (because the total rule count is
  // under the limit).
  RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {kId2}, base::nullopt);
  VerifyPublicRulesetIDs(*extension(), {kId2});
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
  AddRuleset(CreateRuleset(kId2, 10, 10, false));

  RulesetManagerObserver ruleset_waiter(manager());
  LoadAndExpectSuccess();
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);

  std::vector<RulesetSource> static_sources =
      RulesetSource::CreateStatic(*extension());
  ASSERT_EQ(2u, static_sources.size());

  constexpr char kReindexHistogram[] =
      "Extensions.DeclarativeNetRequest.RulesetReindexSuccessful";
  {
    // First delete the indexed ruleset file for the second ruleset. Enabling it
    // should cause re-indexing and succeed in enabling the ruleset.
    base::HistogramTester tester;
    ASSERT_TRUE(base::DeleteFile(static_sources[1].indexed_path()));

    RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {kId2},
                                     base::nullopt);
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

  std::vector<std::string> ruleset_ids;
  RunUpdateEnabledRulesetsFunction(*extension(), {kId1, kId3}, {kId2},
                                   base::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2});

  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId3, kId3},
                                   base::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Ensure no-op calls succeed.
  RunUpdateEnabledRulesetsFunction(*extension(), {}, {kId2, kId3},
                                   base::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {},
                                   base::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(), {kId2, kId3});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Add dynamic rules and ensure that the setEnabledRulesets call doesn't have
  // any effect on the dynamic ruleset. Also ensure that the getEnabledRulesets
  // call excludes the dynamic ruleset ID.
  ASSERT_TRUE(
      RunDynamicRuleUpdateFunction(*extension(), {}, {CreateGenericRule()}));
  VerifyPublicRulesetIDs(*extension(),
                         {kId2, kId3, dnr_api::DYNAMIC_RULESET_ID});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId2, kId3});

  // Ensure enabling a ruleset takes priority over disabling.
  RunUpdateEnabledRulesetsFunction(*extension(), {kId1}, {kId1},
                                   base::nullopt /* expected_error */);
  VerifyPublicRulesetIDs(*extension(),
                         {kId1, kId2, kId3, dnr_api::DYNAMIC_RULESET_ID});
  VerifyGetEnabledRulesetsFunction(*extension(), {kId1, kId2, kId3});

  // Ensure the set of enabled rulesets persists across extension reloads.
  const ExtensionId extension_id = extension()->id();
  service()->DisableExtension(extension_id,
                              disable_reason::DISABLE_USER_ACTION);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(0);

  service()->EnableExtension(extension_id);
  ruleset_waiter.WaitForExtensionsWithRulesetsCount(1);
  const Extension* extension =
      registry()->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
  ASSERT_TRUE(extension);
  VerifyPublicRulesetIDs(*extension,
                         {kId1, kId2, kId3, dnr_api::DYNAMIC_RULESET_ID});
  VerifyGetEnabledRulesetsFunction(*extension, {kId1, kId2, kId3});
}

INSTANTIATE_TEST_SUITE_P(All,
                         SingleRulesetTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));
INSTANTIATE_TEST_SUITE_P(All,
                         MultipleRulesetsTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
