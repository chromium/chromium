// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/parse_info.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

// Fixure testing that declarative rules corresponding to the Declarative Net
// Request API are correctly indexed, for both packed and unpacked
// extensions.
class RuleIndexingTest : public DNRTestBase {
 public:
  RuleIndexingTest() {}

  // DNRTestBase override.
  void SetUp() override {
    DNRTestBase::SetUp();
    loader_ = CreateExtensionLoader();
  }

 protected:
  void AddRule(const TestRule& rule) { rules_list_.push_back(rule); }

  // This takes precedence over the AddRule method.
  void SetRules(std::unique_ptr<base::Value> rules) {
    rules_value_ = std::move(rules);
  }

  // Loads the extension and verifies the indexed ruleset location and histogram
  // counts.
  void LoadAndExpectSuccess(size_t expected_indexed_rules_count) {
    base::HistogramTester tester;
    WriteExtensionData();

    loader_->set_should_fail(false);

    // Clear all load errors before loading the extension.
    error_reporter()->ClearErrors();

    extension_ = loader_->LoadExtension(extension_dir_);
    ASSERT_TRUE(extension_.get());

    EXPECT_TRUE(HasValidIndexedRuleset(*extension_, browser_context()));

    // Ensure no load errors were reported.
    EXPECT_TRUE(error_reporter()->GetErrors()->empty());

    tester.ExpectTotalCount(kIndexRulesTimeHistogram, 1);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 1);
    tester.ExpectBucketCount(kManifestRulesCountHistogram,
                             expected_indexed_rules_count, 1);
  }

  void LoadAndExpectError(const std::string& expected_error) {
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
              errors->at(0).find(base::UTF8ToUTF16(expected_error)))
        << "expected: " << expected_error << " actual: " << errors->at(0);

    tester.ExpectTotalCount(kIndexRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram, 0u);
    tester.ExpectTotalCount(kManifestRulesCountHistogram, 0u);
  }

  void set_persist_invalid_json_file() { persist_invalid_json_file_ = true; }

  void set_persist_initial_indexed_ruleset() {
    persist_initial_indexed_ruleset_ = true;
  }

  ChromeTestExtensionLoader* extension_loader() { return loader_.get(); }

  const Extension* extension() const { return extension_.get(); }
  void set_extension(scoped_refptr<const Extension> extension) {
    extension_ = extension;
  }

 private:
  void WriteExtensionData() {
    extension_dir_ =
        temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension"));

    // Create extension directory.
    EXPECT_TRUE(base::CreateDirectory(extension_dir_));

    if (rules_value_) {
      WriteManifestAndRuleset(extension_dir_, kJSONRulesetFilepath,
                              kJSONRulesFilename, *rules_value_,
                              {} /* hosts */);
    } else {
      WriteManifestAndRuleset(extension_dir_, kJSONRulesetFilepath,
                              kJSONRulesFilename, rules_list_, {} /* hosts */);
    }

    // Overwrite the JSON rules file with some invalid json.
    if (persist_invalid_json_file_) {
      std::string data = "invalid json";
      base::WriteFile(extension_dir_.Append(kJSONRulesetFilepath), data.c_str(),
                      data.size());
    }

    if (persist_initial_indexed_ruleset_) {
      std::string data = "user ruleset";
      base::WriteFile(file_util::GetIndexedRulesetPath(extension_dir_),
                      data.c_str(), data.size());
    }
  }

  LoadErrorReporter* error_reporter() {
    return LoadErrorReporter::GetInstance();
  }

  std::vector<TestRule> rules_list_;
  std::unique_ptr<base::Value> rules_value_;
  base::FilePath extension_dir_;
  std::unique_ptr<ChromeTestExtensionLoader> loader_;
  scoped_refptr<const Extension> extension_;
  bool persist_invalid_json_file_ = false;
  bool persist_initial_indexed_ruleset_ = false;

  DISALLOW_COPY_AND_ASSIGN(RuleIndexingTest);
};

TEST_P(RuleIndexingTest, DuplicateResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types =
      std::vector<std::string>({"image", "stylesheet"});
  rule.condition->excluded_resource_types = std::vector<std::string>({"image"});
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_RESOURCE_TYPE_DUPLICATED, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("https://google.com");
  AddRule(rule);
  LoadAndExpectError(
      ParseInfo(ParseResult::ERROR_EMPTY_REDIRECT_RULE_PRIORITY, 0u)
          .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyRedirectRuleUrl) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID;
  AddRule(rule);

  rule.id = kMinValidID + 1;
  rule.action->type = std::string("redirect");
  rule.priority = kMinValidPriority;
  AddRule(rule);

  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_REDIRECT_URL, 1u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, InvalidRuleID) {
  TestRule rule = CreateGenericRule();
  rule.id = kMinValidID - 1;
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_INVALID_RULE_ID, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, InvalidRedirectRulePriority) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("https://google.com");
  rule.priority = kMinValidPriority - 1;
  AddRule(rule);
  LoadAndExpectError(
      ParseInfo(ParseResult::ERROR_INVALID_REDIRECT_RULE_PRIORITY, 0u)
          .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, NoApplicableResourceTypes) {
  TestRule rule = CreateGenericRule();
  rule.condition->excluded_resource_types = std::vector<std::string>(
      {"main_frame", "sub_frame", "stylesheet", "script", "image", "font",
       "object", "xmlhttprequest", "ping", "csp_report", "media", "websocket",
       "other"});
  AddRule(rule);
  LoadAndExpectError(
      ParseInfo(ParseResult::ERROR_NO_APPLICABLE_RESOURCE_TYPES, 0u)
          .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyDomainsList) {
  TestRule rule = CreateGenericRule();
  rule.condition->domains = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_DOMAINS_LIST, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyResourceTypeList) {
  TestRule rule = CreateGenericRule();
  rule.condition->resource_types = std::vector<std::string>();
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_RESOURCE_TYPES_LIST, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyURLFilter) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string();
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_EMPTY_URL_FILTER, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, InvalidRedirectURL) {
  TestRule rule = CreateGenericRule();
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("google");
  rule.priority = kMinValidPriority;
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_INVALID_REDIRECT_URL, 0u)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, ListNotPassed) {
  SetRules(std::make_unique<base::DictionaryValue>());
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_LIST_NOT_PASSED)
                         .GetErrorDescription(kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, DuplicateIDS) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);
  AddRule(rule);
  LoadAndExpectError(ParseInfo(ParseResult::ERROR_DUPLICATE_IDS, 1u)
                         .GetErrorDescription(kJSONRulesFilename));
}

// Ensure that we limit the number of parse failure warnings shown.
TEST_P(RuleIndexingTest, TooManyParseFailures) {
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

    InstallWarning warning(ErrorUtils::FormatErrorMessage(
        kTooManyParseFailuresWarning,
        std::to_string(kMaxUnparsedRulesWarnings)));
    warning.key = manifest_keys::kDeclarativeNetRequestKey;
    warning.specific = manifest_keys::kDeclarativeRuleResourcesKey;
    EXPECT_EQ(warning, expected_warnings[0]);

    // The subsequent warnings should correspond to the first
    // |kMaxUnparsedRulesWarnings| rules, which couldn't be parsed.
    for (size_t i = 0; i < kMaxUnparsedRulesWarnings; i++) {
      warning.message = ErrorUtils::FormatErrorMessage(kRuleNotParsedWarning,
                                                       std::to_string(i));
      EXPECT_EQ(expected_warnings[i + 1], warning);
    }
  }
}

// Ensures that rules which can't be parsed are ignored and cause an install
// warning.
TEST_P(RuleIndexingTest, InvalidJSONRule) {
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
  LoadAndExpectSuccess(2 /* rules count */);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(2u, extension()->install_warnings().size());
    std::vector<InstallWarning> expected_warnings;

    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(kRuleNotParsedWarning, "1"),
        manifest_keys::kDeclarativeNetRequestKey,
        manifest_keys::kDeclarativeRuleResourcesKey);
    expected_warnings.emplace_back(
        ErrorUtils::FormatErrorMessage(kRuleNotParsedWarning, "3"),
        manifest_keys::kDeclarativeNetRequestKey,
        manifest_keys::kDeclarativeRuleResourcesKey);
    EXPECT_EQ(expected_warnings, extension()->install_warnings());
  }
}

// Ensure that we can add up to MAX_NUMBER_OF_RULES.
TEST_P(RuleIndexingTest, RuleCountLimitMatched) {
  namespace dnr_api = extensions::api::declarative_net_request;
  TestRule rule = CreateGenericRule();
  for (int i = 0; i < dnr_api::MAX_NUMBER_OF_RULES; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }
  LoadAndExpectSuccess(dnr_api::MAX_NUMBER_OF_RULES);
}

// Ensure that we get an install warning on exceeding the rule count limit.
TEST_P(RuleIndexingTest, RuleCountLimitExceeded) {
  namespace dnr_api = extensions::api::declarative_net_request;
  TestRule rule = CreateGenericRule();
  for (int i = 1; i <= dnr_api::MAX_NUMBER_OF_RULES + 1; ++i) {
    rule.id = kMinValidID + i;
    rule.condition->url_filter = std::to_string(i);
    AddRule(rule);
  }

  extension_loader()->set_ignore_manifest_warnings(true);
  LoadAndExpectSuccess(dnr_api::MAX_NUMBER_OF_RULES);

  // TODO(crbug.com/879355): CrxInstaller reloads the extension after moving it,
  // which causes it to lose the install warning. This should be fixed.
  if (GetParam() != ExtensionLoadType::PACKED) {
    ASSERT_EQ(1u, extension()->install_warnings().size());
    EXPECT_EQ(InstallWarning(kRuleCountExceeded,
                             manifest_keys::kDeclarativeNetRequestKey,
                             manifest_keys::kDeclarativeRuleResourcesKey),
              extension()->install_warnings()[0]);
  }
}

TEST_P(RuleIndexingTest, InvalidJSONFile) {
  set_persist_invalid_json_file();
  // The error is returned by the JSON parser we use. Hence just test that it's
  // prepended with |kJSONRulesFilename|.
  LoadAndExpectError(base::StringPrintf("%s: ", kJSONRulesFilename));
}

TEST_P(RuleIndexingTest, EmptyRuleset) {
  LoadAndExpectSuccess(0 /* rules count */);
}

TEST_P(RuleIndexingTest, AddSingleRule) {
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);
}

TEST_P(RuleIndexingTest, AddTwoRules) {
  TestRule rule = CreateGenericRule();
  AddRule(rule);

  rule.id = kMinValidID + 1;
  AddRule(rule);
  LoadAndExpectSuccess(2 /* rules count */);
}

TEST_P(RuleIndexingTest, ReloadExtension) {
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);

  base::HistogramTester tester;
  TestExtensionRegistryObserver registry_observer(registry());

  service()->ReloadExtension(extension()->id());
  // Reloading should invalidate pointers to existing extension(). Hence reset
  // it.
  set_extension(
      base::WrapRefCounted(registry_observer.WaitForExtensionLoaded()));

  // Reloading the extension should cause the rules to be re-indexed in the
  // case of unpacked extensions.
  int expected_histogram_count = -1;
  switch (GetParam()) {
    case ExtensionLoadType::PACKED:
      expected_histogram_count = 0;
      break;
    case ExtensionLoadType::UNPACKED:
      expected_histogram_count = 1;
      break;
  }

  tester.ExpectTotalCount(kIndexRulesTimeHistogram, expected_histogram_count);
  tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                          expected_histogram_count);
  tester.ExpectBucketCount(kManifestRulesCountHistogram, 1 /* rules count */,
                           expected_histogram_count);

  // Ensure no install warnings were raised on reload.
  EXPECT_TRUE(extension()->install_warnings().empty());
}

// Test that we do not use an extension provided indexed ruleset.
TEST_P(RuleIndexingTest, ExtensionWithIndexedRuleset) {
  set_persist_initial_indexed_ruleset();
  AddRule(CreateGenericRule());
  LoadAndExpectSuccess(1 /* rules count */);
}

INSTANTIATE_TEST_CASE_P(,
                        RuleIndexingTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
