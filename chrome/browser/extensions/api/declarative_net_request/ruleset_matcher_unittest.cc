// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/url_pattern_index/flat/url_pattern_index_generated.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {
namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

class RulesetMatcherTest : public DNRTestBase {
 public:
  RulesetMatcherTest() {}

 protected:
  const Extension* extension() const { return extension_.get(); }

  void LoadExtensionWithRules(const std::vector<TestRule>& rules) {
    base::FilePath extension_dir =
        temp_dir().GetPath().Append(FILE_PATH_LITERAL("test_extension"));

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir));
    WriteManifestAndRuleset(extension_dir, kJSONRulesetFilepath,
                            kJSONRulesFilename, rules, {} /* hosts */);

    extension_ = CreateExtensionLoader()->LoadExtension(extension_dir);
    ASSERT_TRUE(extension_);
  }

 private:
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(RulesetMatcherTest);
};

// Tests a simple blocking rule.
TEST_P(RulesetMatcherTest, ShouldBlockRequest) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  int expected_checksum;
  ASSERT_TRUE(
      ExtensionPrefs::Get(browser_context())
          ->GetDNRRulesetChecksum(extension()->id(), &expected_checksum));

  std::unique_ptr<RulesetMatcher> matcher;
  EXPECT_EQ(RulesetMatcher::kLoadSuccess,
            RulesetMatcher::CreateVerifiedMatcher(
                file_util::GetIndexedRulesetPath(extension()->path()),
                expected_checksum, &matcher));

  EXPECT_TRUE(matcher->ShouldBlockRequest(
      GURL("http://google.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true));
  EXPECT_FALSE(matcher->ShouldBlockRequest(
      GURL("http://yahoo.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true));
}

// Tests a simple redirect rule.
TEST_P(RulesetMatcherTest, ShouldRedirectRequest) {
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("google.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("http://yahoo.com");

  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({rule}));

  int expected_checksum;
  ASSERT_TRUE(
      ExtensionPrefs::Get(browser_context())
          ->GetDNRRulesetChecksum(extension()->id(), &expected_checksum));

  std::unique_ptr<RulesetMatcher> matcher;
  EXPECT_EQ(RulesetMatcher::kLoadSuccess,
            RulesetMatcher::CreateVerifiedMatcher(
                file_util::GetIndexedRulesetPath(extension()->path()),
                expected_checksum, &matcher));

  GURL redirect_url;
  EXPECT_TRUE(matcher->ShouldRedirectRequest(
      GURL("http://google.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true, &redirect_url));
  EXPECT_EQ(GURL("http://yahoo.com"), redirect_url);

  EXPECT_FALSE(matcher->ShouldRedirectRequest(
      GURL("http://yahoo.com"), url::Origin(),
      url_pattern_index::flat::ElementType_SUBDOCUMENT, true, &redirect_url));
}

// Tests that a modified ruleset file fails verification.
TEST_P(RulesetMatcherTest, FailedVerification) {
  ASSERT_NO_FATAL_FAILURE(LoadExtensionWithRules({CreateGenericRule()}));

  base::FilePath indexed_ruleset_path =
      file_util::GetIndexedRulesetPath(extension()->path());

  // Persist invalid data to the ruleset file and ensure that a version mismatch
  // occurs.
  std::string data = "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(indexed_ruleset_path, data.c_str(), data.size()));

  int expected_checksum;
  ASSERT_TRUE(
      ExtensionPrefs::Get(browser_context())
          ->GetDNRRulesetChecksum(extension()->id(), &expected_checksum));

  std::unique_ptr<RulesetMatcher> matcher;
  EXPECT_EQ(RulesetMatcher::kLoadErrorVersionMismatch,
            RulesetMatcher::CreateVerifiedMatcher(indexed_ruleset_path,
                                                  expected_checksum, &matcher));

  // Now, persist invalid data to the ruleset file, while maintaining the
  // correct version header. Ensure that it fails verification due to checksum
  // mismatch.
  data = GetVersionHeaderForTesting() + "invalid data";
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(indexed_ruleset_path, data.c_str(), data.size()));
  EXPECT_EQ(RulesetMatcher::kLoadErrorChecksumMismatch,
            RulesetMatcher::CreateVerifiedMatcher(indexed_ruleset_path,
                                                  expected_checksum, &matcher));
}

INSTANTIATE_TEST_CASE_P(,
                        RulesetMatcherTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
