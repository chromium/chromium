// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_features.h"
#include "components/version_info/version_info.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace {

using extensions::ScopedCurrentChannel;

class DeclarativeNetRequestApiTest : public extensions::ExtensionApiTest {
 public:
  DeclarativeNetRequestApiTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        /*disabled_features=*/{features::kHttpsUpgrades});
  }

  ~DeclarativeNetRequestApiTest() override = default;
  DeclarativeNetRequestApiTest(const DeclarativeNetRequestApiTest&) = delete;
  DeclarativeNetRequestApiTest& operator=(const DeclarativeNetRequestApiTest&) =
      delete;

 protected:
  // ExtensionApiTest override.
  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(StartEmbeddedTestServer());

    // Map all hosts to localhost.
    host_resolver()->AddRule("*", "127.0.0.1");

    base::FilePath test_data_dir =
        test_data_dir_.AppendASCII("declarative_net_request");

    // Copy the |test_data_dir| to a temporary location. We do this to ensure
    // that the temporary kMetadata folder created as a result of loading the
    // extension is not written to the src directory and is automatically
    // removed.
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::CopyDirectory(test_data_dir, temp_dir_.GetPath(), true /*recursive*/);

    // Override the path used for loading the extension.
    test_data_dir_ = temp_dir_.GetPath().AppendASCII("declarative_net_request");
  }

 private:
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, DynamicRules) {
  ASSERT_TRUE(RunExtensionTest("dynamic_rules")) << message_;
}

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, RegexRuleMessage) {
  // Ensure the error message for large RegEx rules is updated with the
  // correct value for the memory limit.
  std::string expected_amount = base::StringPrintf(
      "%dKB", extensions::declarative_net_request::kRegexMaxMemKb);
  EXPECT_THAT(extensions::declarative_net_request::kErrorRegexTooLarge,
              testing::HasSubstr(expected_amount));
}

class DeclarativeNetRequestSafeRulesLazyApiTest
    : public DeclarativeNetRequestApiTest {
 public:
  DeclarativeNetRequestSafeRulesLazyApiTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kDeclarativeNetRequestSafeRuleLimits);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on ASAN/MSAN: https://crbug.com/40742546
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_DynamicRulesLimits DISABLED_DynamicRulesLimits
#else
#define MAYBE_DynamicRulesLimits DynamicRulesLimits
#endif
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestSafeRulesLazyApiTest,
                       MAYBE_DynamicRulesLimits) {
  ExtensionTestMessageListener listener("ready", ReplyBehavior::kWillReply);

  // Set up overrides for rule limits and send them to the test extension. This
  // is done because running the test with the actual rule limits will be very
  // slow.
  base::AutoReset<int> dynamic_rule_limit_override =
      extensions::declarative_net_request::
          CreateScopedDynamicRuleLimitOverrideForTesting(200);
  base::AutoReset<int> unsafe_dynamic_rule_limit_override =
      extensions::declarative_net_request::
          CreateScopedUnsafeDynamicRuleLimitOverrideForTesting(50);
  base::AutoReset<int> regex_rule_limit_override = extensions::
      declarative_net_request::CreateScopedRegexRuleLimitOverrideForTesting(50);
  std::string rule_limits = base::StringPrintf(
      R"({"ruleLimit":%d,"unsafeRuleLimit":%d,"regexRuleLimit":%d})", 200, 50,
      50);

  const extensions::Extension* extension =
      LoadExtension(test_data_dir_.AppendASCII("dynamic_rules_limits"));
  ASSERT_TRUE(extension);
  ASSERT_TRUE(listener.WaitUntilSatisfied());

  extensions::ResultCatcher result_catcher;
  listener.Reply(rule_limits);
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, OnRulesMatchedDebug) {
  ASSERT_TRUE(RunExtensionTest("on_rules_matched_debug")) << message_;
}

#if !BUILDFLAG(IS_ANDROID)
// TODO(https://crbug.com/491516661): This test uses an MV2 extension because it
// explicitly exercises capabilities linked to MV2 (webRequestBlocking). This
// can be updated in the future with e.g.  a policy-installed extension. Also,
// Android does not support MV2 extensions, so until this test is updated to
// MV3, skip it on Android.
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, ModifyHeaders) {
  ASSERT_TRUE(RunExtensionTest("modify_headers")) << message_;
}

// TODO(crbug.com/371432155): Port to desktop Android when chrome.tabs API is
// available.
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, TestBrowserSetUserAgent) {
  ASSERT_TRUE(RunExtensionTest("test_browser_set_user_agent")) << message_;
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, GetMatchedRules) {
  ASSERT_TRUE(RunExtensionTest("get_matched_rules")) << message_;
}

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, IsRegexSupported) {
  ASSERT_TRUE(RunExtensionTest("is_regex_supported")) << message_;
}

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, TestMatchOutcome) {
  ASSERT_TRUE(RunExtensionTest("test_match_outcome")) << message_;
}

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiTest, UpdateStaticRules) {
  ASSERT_TRUE(RunExtensionTest("update_static_rules")) << message_;
}

class DeclarativeNetRequestApiFencedFrameTest
    : public DeclarativeNetRequestApiTest {
 protected:
  DeclarativeNetRequestApiFencedFrameTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {{blink::features::kFencedFrames, {{}}},
         {features::kPrivacySandboxAdsAPIsOverride, {}}},
        // TODO(crbug.com/40248833): Use HTTPS URLs in tests to avoid having to
        // disable this feature.
        /*disabled_features=*/{features::kHttpsUpgrades});
    // Fenced frames are only allowed in secure contexts.
    UseHttpsTestServer();
  }

  ~DeclarativeNetRequestApiFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40877906): Re-enable this test
IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiFencedFrameTest, DISABLED_Load) {
  ASSERT_TRUE(RunExtensionTest("fenced_frames")) << message_;
}

class DeclarativeNetRequestApiPrerenderingTest
    : public DeclarativeNetRequestApiTest {
 public:
  DeclarativeNetRequestApiPrerenderingTest() = default;
  ~DeclarativeNetRequestApiPrerenderingTest() override = default;

 private:
  content::test::ScopedPrerenderFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestApiPrerenderingTest,
                       PrerenderedPageInterception) {
  ASSERT_TRUE(RunExtensionTest("prerendering")) << message_;
}

class DeclarativeNetRequestLazyApiResponseHeadersTest
    : public DeclarativeNetRequestApiTest {
 public:
  DeclarativeNetRequestLazyApiResponseHeadersTest() {
    scoped_feature_list_.InitAndEnableFeature(
        extensions_features::kDeclarativeNetRequestResponseHeaderMatching);
  }

 private:
  // TODO(crbug.com/40727004): Once feature is launched to stable and feature
  // flag can be removed, replace usages of this test class with just
  // DeclarativeNetRequestApiTest.
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedCurrentChannel current_channel_override_{version_info::Channel::DEV};
};

IN_PROC_BROWSER_TEST_F(DeclarativeNetRequestLazyApiResponseHeadersTest,
                       TestMatchOutcomeWithResponseHeaders) {
  ASSERT_TRUE(RunExtensionTest("test_match_outcome_response_headers"))
      << message_;
}

}  // namespace
