// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
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

class RulesetManagerTest : public DNRTestBase {
 public:
  RulesetManagerTest() {}

 protected:
  using Action = RulesetManager::Action;

  // Helper to create a ruleset matcher instance for the given |rules|.
  void CreateMatcherForRules(
      const std::vector<TestRule>& rules,
      const std::string& extension_dirname,
      std::unique_ptr<RulesetMatcher>* matcher,
      const std::vector<std::string>& host_permissions = {},
      bool has_background_script = false) {
    base::FilePath extension_dir =
        temp_dir().GetPath().AppendASCII(extension_dirname);

    // Create extension directory.
    ASSERT_TRUE(base::CreateDirectory(extension_dir));
    WriteManifestAndRuleset(extension_dir, kJSONRulesetFilepath,
                            kJSONRulesFilename, rules, host_permissions,
                            has_background_script);

    last_loaded_extension_ =
        CreateExtensionLoader()->LoadExtension(extension_dir);
    ASSERT_TRUE(last_loaded_extension_);

    // This is required since we mock ExtensionSystem in our tests.
    SimulateAddExtensionOnIOThread(last_loaded_extension(),
                                   false /*incognito_enabled*/);

    int expected_checksum;
    EXPECT_TRUE(ExtensionPrefs::Get(browser_context())
                    ->GetDNRRulesetChecksum(last_loaded_extension_->id(),
                                            &expected_checksum));

    EXPECT_EQ(
        RulesetMatcher::kLoadSuccess,
        RulesetMatcher::CreateVerifiedMatcher(
            file_util::GetIndexedRulesetPath(last_loaded_extension_->path()),
            expected_checksum, matcher));
  }

  void SetIncognitoEnabled(const Extension* extension, bool incognito_enabled) {
    util::SetIsIncognitoEnabled(extension->id(), browser_context(),
                                incognito_enabled);

    // This is required since we mock ExtensionSystem in our tests.
    SimulateAddExtensionOnIOThread(extension, incognito_enabled);
  }

  InfoMap* info_map() {
    return ExtensionSystem::Get(browser_context())->info_map();
  }

  const Extension* last_loaded_extension() const {
    return last_loaded_extension_.get();
  }

  // Returns a renderer-initiated request to the given |url|.
  WebRequestInfo GetRequestForURL(base::StringPiece url) {
    const int kRendererId = 1;
    WebRequestInfo info;
    info.url = GURL(url);
    info.render_process_id = kRendererId;
    return info;
  }

 private:
  void SimulateAddExtensionOnIOThread(const Extension* extension,
                                      bool incognito_enabled) {
    info_map()->AddExtension(extension, base::Time(), incognito_enabled,
                             false /*notifications_disabled*/);
  }

  scoped_refptr<const Extension> last_loaded_extension_;

  DISALLOW_COPY_AND_ASSIGN(RulesetManagerTest);
};

// Tests that the RulesetManager handles multiple rulesets correctly.
TEST_P(RulesetManagerTest, MultipleRulesets) {
  enum RulesetMask {
    kEnableRulesetOne = 1 << 0,
    kEnableRulesetTwo = 1 << 1,
  };

  TestRule rule_one = CreateGenericRule();
  rule_one.condition->url_filter = std::string("one.com");

  TestRule rule_two = CreateGenericRule();
  rule_two.condition->url_filter = std::string("two.com");

  RulesetManager* manager = info_map()->GetRulesetManager();
  ASSERT_TRUE(manager);

  WebRequestInfo request_one_info = GetRequestForURL("http://one.com");
  WebRequestInfo request_two_info = GetRequestForURL("http://two.com");
  WebRequestInfo request_three_info = GetRequestForURL("http://three.com");

  auto should_block_request = [manager](const WebRequestInfo& request) {
    GURL redirect_url;
    return manager->EvaluateRequest(request, false /*is_incognito_context*/,
                                    &redirect_url) == Action::BLOCK;
  };

  for (int mask = 0; mask < 4; mask++) {
    SCOPED_TRACE(base::StringPrintf("Testing ruleset mask %d", mask));

    ASSERT_EQ(0u, manager->GetMatcherCountForTest());

    std::string extension_id_one, extension_id_two;
    size_t expected_matcher_count = 0;

    // Add the required rulesets.
    if (mask & kEnableRulesetOne) {
      ++expected_matcher_count;
      std::unique_ptr<RulesetMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_one}, std::to_string(mask) + "_one", &matcher));
      extension_id_one = last_loaded_extension()->id();
      manager->AddRuleset(extension_id_one, std::move(matcher),
                          URLPatternSet());
    }
    if (mask & kEnableRulesetTwo) {
      ++expected_matcher_count;
      std::unique_ptr<RulesetMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_two}, std::to_string(mask) + "_two", &matcher));
      extension_id_two = last_loaded_extension()->id();
      manager->AddRuleset(extension_id_two, std::move(matcher),
                          URLPatternSet());
    }

    ASSERT_EQ(expected_matcher_count, manager->GetMatcherCountForTest());

    EXPECT_EQ((mask & kEnableRulesetOne) != 0,
              should_block_request(request_one_info));
    EXPECT_EQ((mask & kEnableRulesetTwo) != 0,
              should_block_request(request_two_info));
    EXPECT_FALSE(should_block_request(request_three_info));

    // Remove the rulesets.
    if (mask & kEnableRulesetOne)
      manager->RemoveRuleset(extension_id_one);
    if (mask & kEnableRulesetTwo)
      manager->RemoveRuleset(extension_id_two);
  }
}

// Tests that only extensions enabled in incognito mode can modify requests made
// from the incognito context.
TEST_P(RulesetManagerTest, IncognitoRequests) {
  RulesetManager* manager = info_map()->GetRulesetManager();
  ASSERT_TRUE(manager);

  // Add an extension ruleset blocking "example.com".
  TestRule rule_one = CreateGenericRule();
  rule_one.condition->url_filter = std::string("example.com");
  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule_one}, "test_extension", &matcher));
  manager->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                      URLPatternSet());

  WebRequestInfo request_info = GetRequestForURL("http://example.com");

  GURL redirect_url;

  // By default, the extension is disabled in incognito mode. So requests from
  // incognito contexts should not be evaluated.
  EXPECT_FALSE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                        browser_context()));
  EXPECT_EQ(Action::NONE,
            manager->EvaluateRequest(
                request_info, true /*is_incognito_context*/, &redirect_url));
  EXPECT_EQ(Action::BLOCK,
            manager->EvaluateRequest(
                request_info, false /*is_incognito_context*/, &redirect_url));

  // Enabling the extension in incognito mode, should cause requests from
  // incognito contexts to also be evaluated.
  SetIncognitoEnabled(last_loaded_extension(), true /*incognito_enabled*/);
  EXPECT_TRUE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                       browser_context()));
  EXPECT_EQ(Action::BLOCK,
            manager->EvaluateRequest(
                request_info, true /*is_incognito_context*/, &redirect_url));
  EXPECT_EQ(Action::BLOCK,
            manager->EvaluateRequest(
                request_info, false /*is_incognito_context*/, &redirect_url));
}

// Test redirect rules.
TEST_P(RulesetManagerTest, Redirect) {
  RulesetManager* manager = info_map()->GetRulesetManager();
  ASSERT_TRUE(manager);

  // Add an extension ruleset which redirects "example.com" to "google.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect_url = std::string("http://google.com");
  std::unique_ptr<RulesetMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule}, "test_extension", &matcher,
                            {"*://example.com/*", "*://abc.com/*"}));
  manager->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                      URLPatternSet());

  // Create a request to "example.com" with an empty initiator. It should be
  // redirected to "google.com".
  const bool is_incognito_context = false;
  GURL redirect_url1;
  WebRequestInfo request = GetRequestForURL("http://example.com");
  request.initiator = base::nullopt;
  EXPECT_EQ(
      Action::REDIRECT,
      manager->EvaluateRequest(request, is_incognito_context, &redirect_url1));
  EXPECT_EQ(GURL("http://google.com"), redirect_url1);

  // Change the initiator to "xyz.com". It should not be redirected since we
  // don't have host permissions to the request initiator.
  GURL redirect_url2;
  request.initiator = url::Origin::Create(GURL("http://xyz.com"));
  EXPECT_EQ(Action::NONE, manager->EvaluateRequest(
                              request, is_incognito_context, &redirect_url2));

  // Change the initiator to "abc.com". It should be redirected since we have
  // the required host permissions.
  GURL redirect_url3;
  request.initiator = url::Origin::Create(GURL("http://abc.com"));
  EXPECT_EQ(
      Action::REDIRECT,
      manager->EvaluateRequest(request, is_incognito_context, &redirect_url3));
  EXPECT_EQ(GURL("http://google.com"), redirect_url3);

  // Ensure web-socket requests are not redirected.
  GURL redirect_url4;
  request = GetRequestForURL("ws://example.com");
  request.initiator = base::nullopt;
  EXPECT_EQ(Action::NONE, manager->EvaluateRequest(
                              request, is_incognito_context, &redirect_url4));
}

// Tests that an extension can't block or redirect resources on the chrome-
// extension scheme.
TEST_P(RulesetManagerTest, ExtensionScheme) {
  RulesetManager* manager = info_map()->GetRulesetManager();
  ASSERT_TRUE(manager);

  const Extension* extension_1 = nullptr;
  const Extension* extension_2 = nullptr;
  // Add an extension with a background page which blocks all requests.
  {
    std::unique_ptr<RulesetMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension", &matcher,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        true /* has_background_script*/));
    extension_1 = last_loaded_extension();
    manager->AddRuleset(extension_1->id(), std::move(matcher), URLPatternSet());
  }

  // Add another extension with a background page which redirects all requests
  // to "http://google.com".
  {
    std::unique_ptr<RulesetMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.priority = kMinValidPriority;
    rule.action->type = std::string("redirect");
    rule.action->redirect_url = std::string("http://google.com");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension_2", &matcher,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        true /* has_background_script*/));
    extension_2 = last_loaded_extension();
    manager->AddRuleset(extension_2->id(), std::move(matcher), URLPatternSet());
  }

  EXPECT_EQ(2u, manager->GetMatcherCountForTest());

  // Ensure that "http://example.com" will be blocked (with blocking taking
  // priority over redirection).
  WebRequestInfo request = GetRequestForURL("http://example.com");
  GURL redirect_url;
  EXPECT_EQ(Action::BLOCK,
            manager->EvaluateRequest(request, false /*is_incognito_context*/,
                                     &redirect_url));

  // Ensure that the background page for |extension_1| won't be blocked or
  // redirected.
  GURL background_page_url_1 = BackgroundInfo::GetBackgroundURL(extension_1);
  EXPECT_TRUE(!background_page_url_1.is_empty());
  request = GetRequestForURL(background_page_url_1.spec());
  EXPECT_EQ(Action::NONE,
            manager->EvaluateRequest(request, false /*is_incognito_context*/,
                                     &redirect_url));

  // Ensure that the background page for |extension_2| won't be blocked or
  // redirected.
  GURL background_page_url_2 = BackgroundInfo::GetBackgroundURL(extension_2);
  EXPECT_TRUE(!background_page_url_2.is_empty());
  request = GetRequestForURL(background_page_url_2.spec());
  EXPECT_EQ(Action::NONE,
            manager->EvaluateRequest(request, false /*is_incognito_context*/,
                                     &redirect_url));

  // Also ensure that an arbitrary url on the chrome extension scheme is also
  // not blocked or redirected.
  request = GetRequestForURL(base::StringPrintf("%s://%s/%s", kExtensionScheme,
                                                "extension_id", "path"));
  EXPECT_EQ(Action::NONE,
            manager->EvaluateRequest(request, false /*is_incognito_context*/,
                                     &redirect_url));
}

TEST_P(RulesetManagerTest, PageAllowingAPI) {
  RulesetManager* manager = info_map()->GetRulesetManager();
  ASSERT_TRUE(manager);

  // Add an extension which blocks all requests except for requests from
  // http://google.com/allow* which are allowed.
  {
    std::unique_ptr<RulesetMatcher> matcher;

    // This blocks all subresource requests. By default the main-frame resource
    // type is excluded.
    TestRule rule1 = CreateGenericRule();
    rule1.id = kMinValidID;
    rule1.condition->url_filter = std::string("*");

    // Also block main frame requests.
    TestRule rule2 = CreateGenericRule();
    rule2.id = kMinValidID + 1;
    rule2.condition->url_filter = std::string("*");
    rule2.condition->resource_types = std::vector<std::string>({"main_frame"});

    ASSERT_NO_FATAL_FAILURE(
        CreateMatcherForRules({rule1, rule2}, "test extension", &matcher, {},
                              true /* background_script */));

    URLPatternSet pattern_set(
        {URLPattern(URLPattern::SCHEME_ALL, "http://google.com/allow*")});
    manager->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        std::move(pattern_set));
  }

  constexpr int kDummyFrameRoutingId = 2;
  constexpr int kDummyFrameId = 3;
  constexpr int kDummyParentFrameId = 1;
  constexpr int kDummyTabId = 5;
  constexpr int kDummyWindowId = 7;
  constexpr char kAllowedPageURL[] = "http://google.com/allow123";

  struct FrameDataParams {
    int frame_id;
    int parent_frame_id;
    std::string last_committed_main_frame_url;
    base::Optional<std::string> pending_main_frame_url;
  };
  struct TestCase {
    std::string url;
    content::ResourceType type;
    base::Optional<std::string> initiator;
    int frame_routing_id;
    base::Optional<FrameDataParams> frame_data_params;
    bool expect_blocked_with_allowed_pages;
  } test_cases[] = {
      // Main frame requests. Allowed based on request url.
      {kAllowedPageURL, content::RESOURCE_TYPE_MAIN_FRAME, base::nullopt,
       MSG_ROUTING_NONE,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://google.com/xyz", base::nullopt}),
       false},
      {"http://google.com/xyz", content::RESOURCE_TYPE_MAIN_FRAME,
       base::nullopt, MSG_ROUTING_NONE,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, base::nullopt}),
       true},

      // Non-navigation browser or service worker request. Not allowed,
      // since the request doesn't correspond to a frame.
      {"http://google.com/xyz", content::RESOURCE_TYPE_SCRIPT, base::nullopt,
       MSG_ROUTING_NONE, base::nullopt, true},

      // Renderer requests - with no |pending_main_frame_url|. Allowed based
      // on the |last_committed_main_frame_url|.
      {kAllowedPageURL, content::RESOURCE_TYPE_SCRIPT, "http://google.com",
       kDummyFrameRoutingId,
       FrameDataParams({kDummyFrameId, kDummyParentFrameId,
                        "http://google.com/xyz", base::nullopt}),
       true},
      {"http://google.com/xyz", content::RESOURCE_TYPE_SCRIPT,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({kDummyFrameId, kDummyParentFrameId, kAllowedPageURL,
                        base::nullopt}),
       false},

      // Renderer requests with |pending_main_frame_url|. This only happens for
      // main frame subresource requests.

      // Here we'll determine "http://example.com/xyz" to be the main frame url
      // due to the origin.
      {"http://example.com/script.js", content::RESOURCE_TYPE_SCRIPT,
       "http://example.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, "http://example.com/xyz"}),
       true},

      // Here we'll determine |kAllowedPageURL| to be the main
      // frame url due to the origin.
      {"http://example.com/script.js", content::RESOURCE_TYPE_SCRIPT,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, "http://yahoo.com/xyz"}),
       false},

      // In these cases both |pending_main_frame_url| and
      // |last_committed_main_frame_url| will be tested since we won't be able
      // to determine the correct top level frame url using origin.
      {"http://example.com/script.js", content::RESOURCE_TYPE_SCRIPT,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://google.com/abc", kAllowedPageURL}),
       false},
      {"http://example.com/script.js", content::RESOURCE_TYPE_SCRIPT,
       base::nullopt, kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, "http://google.com/abc"}),
       false},
      {"http://example.com/script.js", content::RESOURCE_TYPE_SCRIPT,
       base::nullopt, kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://yahoo.com/abc", "http://yahoo.com/allow123"}),
       true},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    const TestCase test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Testing case number %zu with url %s",
                                    i + 1, test_case.url.c_str()));

    WebRequestInfo info = GetRequestForURL(test_case.url);
    ASSERT_TRUE(info.url.is_valid());
    info.type = test_case.type;

    if (test_case.initiator)
      info.initiator = url::Origin::Create(GURL(*test_case.initiator));

    info.frame_id = test_case.frame_routing_id;

    if (test_case.frame_data_params) {
      const FrameDataParams& params = *test_case.frame_data_params;
      info.frame_data = ExtensionApiFrameIdMap::FrameData(
          params.frame_id, params.parent_frame_id, kDummyTabId, kDummyWindowId,
          GURL(params.last_committed_main_frame_url));
      if (params.pending_main_frame_url)
        info.frame_data->pending_main_frame_url =
            GURL(*params.pending_main_frame_url);
    }

    GURL redirect_url;
    RulesetManager::Action expected_action =
        test_case.expect_blocked_with_allowed_pages
            ? RulesetManager::Action::BLOCK
            : RulesetManager::Action::NONE;
    EXPECT_EQ(expected_action,
              manager->EvaluateRequest(info, false /*is_incognito_context*/,
                                       &redirect_url));
  }
}

TEST_P(RulesetManagerTest, HostPermissionForInitiator) {
  RulesetManager* manager = info_map()->GetRulesetManager();
  ASSERT_TRUE(manager);

  // Add an extension which redirects all sub-resource and sub-frame requests
  // made to example.com, to foo.com. By default, the "main_frame" type is
  // excluded if no "resource_types" are specified.
  std::unique_ptr<RulesetMatcher> redirect_matcher;
  {
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = std::string("example.com");
    rule.action->type = std::string("redirect");
    rule.action->redirect_url = std::string("https://foo.com");
    std::vector<std::string> host_permissions = {"*://yahoo.com/*",
                                                 "*://example.com/*"};
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "redirecting extension", &redirect_matcher, host_permissions,
        false /* has_background_script */));
  }
  std::string redirect_extension_id = last_loaded_extension()->id();

  // Add an extension which blocks all sub-resource and sub-frame requests to
  // example.com. By default, the "main_frame" type is excluded if no
  // "resource_types" are specified. The extension has no host permissions.
  std::unique_ptr<RulesetMatcher> blocking_matcher;
  {
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rule.condition->url_filter = std::string("example.com");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "blocking extension", &blocking_matcher,
        {} /* host_permissions */, false /* has_background_script */));
  }
  std::string blocking_extension_id = last_loaded_extension()->id();

  struct TestCase {
    std::string url;
    base::Optional<url::Origin> initiator;
    Action expected_action_redirect_extension;
    Action expected_action_blocking_extension;
  } cases[] = {
      // Empty initiator. Has access.
      {"https://example.com", base::nullopt, Action::REDIRECT, Action::BLOCK},
      // Opaque origin as initiator. Has access.
      {"https://example.com", url::Origin(), Action::REDIRECT, Action::BLOCK},
      // yahoo.com as initiator. Has access.
      {"https://example.com", url::Origin::Create(GURL("http://yahoo.com")),
       Action::REDIRECT, Action::BLOCK},
      // No matching rule.
      {"https://yahoo.com", url::Origin::Create(GURL("http://example.com")),
       Action::NONE, Action::NONE},
      // Doesn't have access to initiator. But blocking a request doesn't
      // require host permissions.
      {"https://example.com", url::Origin::Create(GURL("http://google.com")),
       Action::NONE, Action::BLOCK},
  };

  auto verify_test_case = [this, manager](
                              const std::string& url,
                              const base::Optional<url::Origin>& initiator,
                              Action expected_action, GURL* redirect_url) {
    SCOPED_TRACE(base::StringPrintf(
        "Url-%s initiator-%s", url.c_str(),
        initiator ? initiator->Serialize().c_str() : "empty"));

    WebRequestInfo request = GetRequestForURL(url);
    request.initiator = initiator;

    bool is_incognito_context = false;
    EXPECT_EQ(
        expected_action,
        manager->EvaluateRequest(request, is_incognito_context, redirect_url));
  };

  // Test redirect extension.
  {
    SCOPED_TRACE("Testing redirect extension");
    manager->AddRuleset(redirect_extension_id, std::move(redirect_matcher),
                        URLPatternSet());
    for (const auto& test : cases) {
      GURL redirect_url;
      verify_test_case(test.url, test.initiator,
                       test.expected_action_redirect_extension, &redirect_url);
      if (test.expected_action_redirect_extension == Action::REDIRECT)
        EXPECT_EQ("https://foo.com/", redirect_url.spec());
    }
    manager->RemoveRuleset(redirect_extension_id);
  }

  // Test blocking extension.
  {
    SCOPED_TRACE("Testing blocking extension");
    manager->AddRuleset(blocking_extension_id, std::move(blocking_matcher),
                        URLPatternSet());
    for (const auto& test : cases) {
      GURL redirect_url;
      verify_test_case(test.url, test.initiator,
                       test.expected_action_blocking_extension, &redirect_url);
    }
    manager->RemoveRuleset(blocking_extension_id);
  }
}

INSTANTIATE_TEST_CASE_P(,
                        RulesetManagerTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
