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
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/api/declarative_net_request.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {

namespace dnr_api = api::declarative_net_request;

namespace {

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

class RulesetManagerTest : public DNRTestBase {
 public:
  RulesetManagerTest() {}

  void SetUp() override {
    DNRTestBase::SetUp();
    manager_ = std::make_unique<RulesetManager>(browser_context());
  }

 protected:
  using RequestActionType = RequestAction::Type;

  // Helper to create a composite matcher instance for the given |rules|.
  void CreateMatcherForRules(
      const std::vector<TestRule>& rules,
      const std::string& extension_dirname,
      std::unique_ptr<CompositeMatcher>* matcher,
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

    ExtensionRegistry::Get(browser_context())
        ->AddEnabled(last_loaded_extension_);

    int expected_checksum;
    EXPECT_TRUE(ExtensionPrefs::Get(browser_context())
                    ->GetDNRRulesetChecksum(last_loaded_extension_->id(),
                                            &expected_checksum));

    std::vector<std::unique_ptr<RulesetMatcher>> matchers(1);
    EXPECT_EQ(RulesetMatcher::kLoadSuccess,
              RulesetMatcher::CreateVerifiedMatcher(
                  RulesetSource::CreateStatic(*last_loaded_extension_),
                  expected_checksum, &matchers[0]));

    *matcher = std::make_unique<CompositeMatcher>(std::move(matchers),
                                                  nullptr /* action_tracker */);
  }

  void SetIncognitoEnabled(const Extension* extension, bool incognito_enabled) {
    util::SetIsIncognitoEnabled(extension->id(), browser_context(),
                                incognito_enabled);
    ExtensionRegistry::Get(browser_context())->AddEnabled(extension);
  }

  const Extension* last_loaded_extension() const {
    return last_loaded_extension_.get();
  }

  // Returns renderer-initiated request params for the given |url|.
  WebRequestInfoInitParams GetRequestParamsForURL(
      base::StringPiece url,
      base::Optional<url::Origin> initiator = base::nullopt) {
    const int kRendererId = 1;
    WebRequestInfoInitParams info;
    info.url = GURL(url);
    info.render_process_id = kRendererId;
    info.initiator = std::move(initiator);
    return info;
  }

  // Returns renderer-initiated request params for the given |url| and
  // |request_headers| request headers.
  WebRequestInfoInitParams GetRequestParamsForURLWithHeaders(
      base::StringPiece url,
      const std::vector<std::string>& request_headers) {
    const int kRendererId = 1;
    WebRequestInfoInitParams info;
    info.url = GURL(url);
    info.render_process_id = kRendererId;

    net::HttpRequestHeaders extra_request_headers;
    for (const auto& header : request_headers)
      extra_request_headers.SetHeaderIfMissing(header, "foo");

    info.extra_request_headers = extra_request_headers;
    return info;
  }

  RulesetManager* manager() { return manager_.get(); }

 private:
  scoped_refptr<const Extension> last_loaded_extension_;
  std::unique_ptr<RulesetManager> manager_;

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

  auto should_block_request = [this](const WebRequestInfo& request, int rule_id,
                                     const ExtensionId& extension_id) {
    manager()->EvaluateRequest(request, false /*is_incognito_context*/);
    return !request.dnr_actions->empty() &&
           ((*request.dnr_actions)[0] ==
            RequestAction(RequestActionType::BLOCK, rule_id, kDefaultPriority,
                          dnr_api::SOURCE_TYPE_MANIFEST, extension_id));
  };

  for (int mask = 0; mask < 4; mask++) {
    SCOPED_TRACE(base::StringPrintf("Testing ruleset mask %d", mask));

    ASSERT_EQ(0u, manager()->GetMatcherCountForTest());

    std::string extension_id_one, extension_id_two;
    size_t expected_matcher_count = 0;

    // Add the required rulesets.
    if (mask & kEnableRulesetOne) {
      ++expected_matcher_count;
      std::unique_ptr<CompositeMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_one}, std::to_string(mask) + "_one", &matcher));
      extension_id_one = last_loaded_extension()->id();
      manager()->AddRuleset(extension_id_one, std::move(matcher),
                            URLPatternSet());
    }
    if (mask & kEnableRulesetTwo) {
      ++expected_matcher_count;
      std::unique_ptr<CompositeMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_two}, std::to_string(mask) + "_two", &matcher));
      extension_id_two = last_loaded_extension()->id();
      manager()->AddRuleset(extension_id_two, std::move(matcher),
                            URLPatternSet());
    }

    ASSERT_EQ(expected_matcher_count, manager()->GetMatcherCountForTest());

    WebRequestInfo request_one_info(GetRequestParamsForURL("http://one.com"));
    WebRequestInfo request_two_info(GetRequestParamsForURL("http://two.com"));
    WebRequestInfo request_three_info(
        GetRequestParamsForURL("http://three.com"));

    EXPECT_EQ(
        (mask & kEnableRulesetOne) != 0,
        should_block_request(request_one_info, *rule_one.id, extension_id_one));
    EXPECT_EQ(
        (mask & kEnableRulesetTwo) != 0,
        should_block_request(request_two_info, *rule_two.id, extension_id_two));
    EXPECT_FALSE(should_block_request(request_three_info, 0 /* rule_id */,
                                      "" /* extension_id */));

    // Remove the rulesets.
    if (mask & kEnableRulesetOne)
      manager()->RemoveRuleset(extension_id_one);
    if (mask & kEnableRulesetTwo)
      manager()->RemoveRuleset(extension_id_two);
  }
}

// Tests that only extensions enabled in incognito mode can modify requests made
// from the incognito context.
TEST_P(RulesetManagerTest, IncognitoRequests) {
  // Add an extension ruleset blocking "example.com".
  TestRule rule_one = CreateGenericRule();
  rule_one.condition->url_filter = std::string("example.com");
  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule_one}, "test_extension", &matcher));
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        URLPatternSet());

  WebRequestInfo request_info(GetRequestParamsForURL("http://example.com"));

  // By default, the extension is disabled in incognito mode. So requests from
  // incognito contexts should not be evaluated.
  EXPECT_FALSE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                        browser_context()));

  manager()->EvaluateRequest(request_info, true /*is_incognito_context*/);
  EXPECT_TRUE(request_info.dnr_actions->empty());
  request_info.dnr_actions.reset();

  manager()->EvaluateRequest(request_info, false /*is_incognito_context*/);
  ASSERT_EQ(1u, request_info.dnr_actions->size());
  EXPECT_EQ(RequestAction(RequestActionType::BLOCK, *rule_one.id,
                          kDefaultPriority, dnr_api::SOURCE_TYPE_MANIFEST,
                          last_loaded_extension()->id()),
            (*request_info.dnr_actions)[0]);
  request_info.dnr_actions.reset();

  // Enabling the extension in incognito mode, should cause requests from
  // incognito contexts to also be evaluated.
  SetIncognitoEnabled(last_loaded_extension(), true /*incognito_enabled*/);
  EXPECT_TRUE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                       browser_context()));

  manager()->EvaluateRequest(request_info, true /*is_incognito_context*/);
  ASSERT_EQ(1u, request_info.dnr_actions->size());
  EXPECT_EQ(RequestAction(RequestActionType::BLOCK, *rule_one.id,
                          kDefaultPriority, dnr_api::SOURCE_TYPE_MANIFEST,
                          last_loaded_extension()->id()),
            (*request_info.dnr_actions)[0]);
  request_info.dnr_actions.reset();

  manager()->EvaluateRequest(request_info, false /*is_incognito_context*/);
  ASSERT_EQ(1u, request_info.dnr_actions->size());
  EXPECT_EQ(RequestAction(RequestActionType::BLOCK, *rule_one.id,
                          kDefaultPriority, dnr_api::SOURCE_TYPE_MANIFEST,
                          last_loaded_extension()->id()),
            (*request_info.dnr_actions)[0]);
  request_info.dnr_actions.reset();
}

// Tests that
// Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions2
// is only emitted when there are active rulesets.
TEST_P(RulesetManagerTest, TotalEvaluationTimeHistogram) {
  WebRequestInfo example_com_request(
      GetRequestParamsForURL("http://example.com"));
  WebRequestInfo google_com_request(
      GetRequestParamsForURL("http://google.com"));
  bool is_incognito_context = false;
  const char* kHistogramName =
      "Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions2";
  {
    base::HistogramTester tester;

    manager()->EvaluateRequest(example_com_request, is_incognito_context);
    EXPECT_TRUE(example_com_request.dnr_actions->empty());

    manager()->EvaluateRequest(google_com_request, is_incognito_context);
    EXPECT_TRUE(google_com_request.dnr_actions->empty());

    tester.ExpectTotalCount(kHistogramName, 0);
    example_com_request.dnr_actions.reset();
    google_com_request.dnr_actions.reset();
  }

  // Add an extension ruleset which blocks requests to "example.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule}, "test_extension", &matcher));
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        URLPatternSet());

  {
    base::HistogramTester tester;

    manager()->EvaluateRequest(example_com_request, is_incognito_context);
    ASSERT_EQ(1u, example_com_request.dnr_actions->size());
    EXPECT_EQ(RequestAction(RequestActionType::BLOCK, *rule.id,
                            kDefaultPriority, dnr_api::SOURCE_TYPE_MANIFEST,
                            last_loaded_extension()->id()),
              (*example_com_request.dnr_actions)[0]);

    tester.ExpectTotalCount(kHistogramName, 1);

    manager()->EvaluateRequest(google_com_request, is_incognito_context);
    EXPECT_TRUE(google_com_request.dnr_actions->empty());

    tester.ExpectTotalCount(kHistogramName, 2);
    example_com_request.dnr_actions.reset();
    google_com_request.dnr_actions.reset();
  }
}

// Test redirect rules.
TEST_P(RulesetManagerTest, Redirect) {
  // Add an extension ruleset which redirects "example.com" to "google.com".
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("example.com");
  rule.priority = kMinValidPriority;
  rule.action->type = std::string("redirect");
  rule.action->redirect.emplace();
  rule.action->redirect->url = std::string("http://google.com");
  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(
      CreateMatcherForRules({rule}, "test_extension", &matcher,
                            {"*://example.com/*", "*://abc.com/*"}));
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                        URLPatternSet());

  // Create a request to "example.com" with an empty initiator. It should be
  // redirected to "google.com".
  const bool is_incognito_context = false;
  const char* kExampleURL = "http://example.com";
  RequestAction expected_redirect_action(
      RequestActionType::REDIRECT, *rule.id, *rule.priority,
      dnr_api::SOURCE_TYPE_MANIFEST, last_loaded_extension()->id());
  expected_redirect_action.redirect_url = GURL("http://google.com");
  WebRequestInfo request_1(GetRequestParamsForURL(kExampleURL, base::nullopt));
  manager()->EvaluateRequest(request_1, is_incognito_context);
  ASSERT_EQ(1u, request_1.dnr_actions->size());
  EXPECT_EQ(expected_redirect_action, (*request_1.dnr_actions)[0]);

  // Change the initiator to "xyz.com". It should not be redirected since we
  // don't have host permissions to the request initiator.
  WebRequestInfo request_2(GetRequestParamsForURL(
      kExampleURL, url::Origin::Create(GURL("http://xyz.com"))));
  manager()->EvaluateRequest(request_2, is_incognito_context);
  EXPECT_TRUE(request_2.dnr_actions->empty());

  // Change the initiator to "abc.com". It should be redirected since we have
  // the required host permissions.
  WebRequestInfo request_3(GetRequestParamsForURL(
      kExampleURL, url::Origin::Create(GURL("http://abc.com"))));
  manager()->EvaluateRequest(request_3, is_incognito_context);
  ASSERT_EQ(1u, request_3.dnr_actions->size());
  EXPECT_EQ(expected_redirect_action, (*request_3.dnr_actions)[0]);

  // Ensure web-socket requests are not redirected.
  WebRequestInfo request_4(
      GetRequestParamsForURL("ws://example.com", base::nullopt));
  manager()->EvaluateRequest(request_4, is_incognito_context);
  EXPECT_TRUE(request_4.dnr_actions->empty());
}

// Tests that an extension can't block or redirect resources on the chrome-
// extension scheme.
TEST_P(RulesetManagerTest, ExtensionScheme) {
  const Extension* extension_1 = nullptr;
  const Extension* extension_2 = nullptr;
  // Add an extension with a background page which blocks all requests.
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension", &matcher,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        true /* has_background_script*/));
    extension_1 = last_loaded_extension();
    manager()->AddRuleset(extension_1->id(), std::move(matcher),
                          URLPatternSet());
  }

  // Add another extension with a background page which redirects all requests
  // to "http://google.com".
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.priority = kMinValidPriority;
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->url = std::string("http://google.com");
    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension_2", &matcher,
        std::vector<std::string>({URLPattern::kAllUrlsPattern}),
        true /* has_background_script*/));
    extension_2 = last_loaded_extension();
    manager()->AddRuleset(extension_2->id(), std::move(matcher),
                          URLPatternSet());
  }

  EXPECT_EQ(2u, manager()->GetMatcherCountForTest());

  // Ensure that "http://example.com" will be blocked (with blocking taking
  // priority over redirection).
  WebRequestInfo request_1(GetRequestParamsForURL("http://example.com"));
  manager()->EvaluateRequest(request_1, false /*is_incognito_context*/);
  ASSERT_EQ(1u, request_1.dnr_actions->size());
  EXPECT_EQ(
      RequestAction(RequestActionType::BLOCK, kMinValidID, kDefaultPriority,
                    dnr_api::SOURCE_TYPE_MANIFEST, extension_1->id()),
      (*request_1.dnr_actions)[0]);

  // Ensure that the background page for |extension_1| won't be blocked or
  // redirected.
  GURL background_page_url_1 = BackgroundInfo::GetBackgroundURL(extension_1);
  EXPECT_TRUE(!background_page_url_1.is_empty());
  WebRequestInfo request_2(
      GetRequestParamsForURL(background_page_url_1.spec()));
  manager()->EvaluateRequest(request_2, false /*is_incognito_context*/);
  EXPECT_TRUE(request_2.dnr_actions->empty());

  // Ensure that the background page for |extension_2| won't be blocked or
  // redirected.
  GURL background_page_url_2 = BackgroundInfo::GetBackgroundURL(extension_2);
  EXPECT_TRUE(!background_page_url_2.is_empty());
  WebRequestInfo request_3(
      GetRequestParamsForURL(background_page_url_2.spec()));
  manager()->EvaluateRequest(request_3, false /*is_incognito_context*/);
  EXPECT_TRUE(request_3.dnr_actions->empty());

  // Also ensure that an arbitrary url on the chrome extension scheme is also
  // not blocked or redirected.
  WebRequestInfo request_4(GetRequestParamsForURL(base::StringPrintf(
      "%s://%s/%s", kExtensionScheme, "extension_id", "path")));
  manager()->EvaluateRequest(request_4, false /*is_incognito_context*/);
  EXPECT_TRUE(request_4.dnr_actions->empty());
}

// Test that headers to be removed in removeHeaders rules are attributed to the
// correct extension.
TEST_P(RulesetManagerTest, RemoveHeaders) {
  const Extension* extension_1 = nullptr;
  const Extension* extension_2 = nullptr;
  // Add an extension with a background page which removes the "cookie" and
  // "setCookie" headers.
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.action->type = std::string("removeHeaders");
    rule.action->remove_headers_list =
        std::vector<std::string>({"cookie", "setCookie"});

    ASSERT_NO_FATAL_FAILURE(
        CreateMatcherForRules({rule}, "test extension", &matcher));
    extension_1 = last_loaded_extension();
    manager()->AddRuleset(extension_1->id(), std::move(matcher),
                          URLPatternSet());
  }

  // Add another extension with a background page which removes the "cookie" and
  // "referer" headers.
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.action->type = std::string("removeHeaders");
    rule.action->remove_headers_list =
        std::vector<std::string>({"cookie", "referer"});

    ASSERT_NO_FATAL_FAILURE(
        CreateMatcherForRules({rule}, "test extension 2", &matcher));
    extension_2 = last_loaded_extension();
    manager()->AddRuleset(extension_2->id(), std::move(matcher),
                          URLPatternSet());
  }

  EXPECT_EQ(2u, manager()->GetMatcherCountForTest());

  // Create a request with the "cookie" and "referer" request headers, and the
  // "set-cookie" response header.
  WebRequestInfo request_1(GetRequestParamsForURLWithHeaders(
      "http://example.com", std::vector<std::string>({"cookie", "referer"})));
  request_1.response_headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders("HTTP/1.1 200 OK\r\n"
                                        "Content-Type: text/plain; UTF-8\r\n"
                                        "Set-Cookie: custom/value\r\n"));

  const std::vector<RequestAction>& actual_actions =
      manager()->EvaluateRequest(request_1, false /*is_incognito_context*/);
  ASSERT_EQ(2u, actual_actions.size());

  // Removal of the cookie header should be attributed to |extension_2| because
  // it was installed later than |extension_1| and thus has more priority.
  RequestAction expected_action_1(
      RequestActionType::REMOVE_HEADERS, kMinValidID, kDefaultPriority,
      dnr_api::SOURCE_TYPE_MANIFEST, extension_2->id());
  expected_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kCookie);

  // Removal of the referer header should be attributed to |extension_2|.
  expected_action_1.request_headers_to_remove.push_back(
      net::HttpRequestHeaders::kReferer);

  RequestAction expected_action_2(
      RequestActionType::REMOVE_HEADERS, kMinValidID, kDefaultPriority,
      dnr_api::SOURCE_TYPE_MANIFEST, extension_1->id());
  expected_action_2.response_headers_to_remove.push_back("set-cookie");

  EXPECT_EQ(expected_action_1, actual_actions[0]);
  EXPECT_EQ(expected_action_2, actual_actions[1]);
}

TEST_P(RulesetManagerTest, PageAllowingAPI) {
  // Add an extension which blocks all requests except for requests from
  // http://google.com/allow* which are allowed.
  {
    std::unique_ptr<CompositeMatcher> matcher;

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
    manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher),
                          std::move(pattern_set));
  }

  int rule_1_id = kMinValidID;
  int rule_2_id = kMinValidID + 1;

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
    base::Optional<GURL> pending_main_frame_url;
  };
  struct TestCase {
    std::string url;
    content::ResourceType type;
    base::Optional<std::string> initiator;
    int frame_routing_id;
    base::Optional<FrameDataParams> frame_data_params;
    bool expect_blocked_with_allowed_pages;
    base::Optional<int> matched_rule_id;
  } test_cases[] = {
      // Main frame requests. Allowed based on request url.
      {kAllowedPageURL, content::ResourceType::kMainFrame, base::nullopt,
       MSG_ROUTING_NONE,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://google.com/xyz", base::nullopt}),
       false, base::nullopt},
      {"http://google.com/xyz", content::ResourceType::kMainFrame,
       base::nullopt, MSG_ROUTING_NONE,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, base::nullopt}),
       true, rule_2_id},

      // Non-navigation browser or service worker request. Not allowed,
      // since the request doesn't correspond to a frame.
      {"http://google.com/xyz", content::ResourceType::kScript, base::nullopt,
       MSG_ROUTING_NONE, base::nullopt, true, rule_1_id},

      // Renderer requests - with no |pending_main_frame_url|. Allowed based
      // on the |last_committed_main_frame_url|.
      {kAllowedPageURL, content::ResourceType::kScript, "http://google.com",
       kDummyFrameRoutingId,
       FrameDataParams({kDummyFrameId, kDummyParentFrameId,
                        "http://google.com/xyz", base::nullopt}),
       true, rule_1_id},
      {"http://google.com/xyz", content::ResourceType::kScript,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({kDummyFrameId, kDummyParentFrameId, kAllowedPageURL,
                        base::nullopt}),
       false, base::nullopt},

      // Renderer requests with |pending_main_frame_url|. This only happens for
      // main frame subresource requests.

      // Here we'll determine "http://example.com/xyz" to be the main frame url
      // due to the origin.
      {"http://example.com/script.js", content::ResourceType::kScript,
       "http://example.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, GURL("http://example.com/xyz")}),
       true, rule_1_id},

      // Here we'll determine |kAllowedPageURL| to be the main
      // frame url due to the origin.
      {"http://example.com/script.js", content::ResourceType::kScript,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, GURL("http://yahoo.com/xyz")}),
       false, base::nullopt},

      // In these cases both |pending_main_frame_url| and
      // |last_committed_main_frame_url| will be tested since we won't be able
      // to determine the correct top level frame url using origin.
      {"http://example.com/script.js", content::ResourceType::kScript,
       "http://google.com", kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://google.com/abc", GURL(kAllowedPageURL)}),
       false, base::nullopt},
      {"http://example.com/script.js", content::ResourceType::kScript,
       base::nullopt, kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        kAllowedPageURL, GURL("http://google.com/abc")}),
       false, base::nullopt},
      {"http://example.com/script.js", content::ResourceType::kScript,
       base::nullopt, kDummyFrameRoutingId,
       FrameDataParams({ExtensionApiFrameIdMap::kTopFrameId,
                        ExtensionApiFrameIdMap::kInvalidFrameId,
                        "http://yahoo.com/abc",
                        GURL("http://yahoo.com/allow123")}),
       true, rule_1_id},
  };

  for (size_t i = 0; i < base::size(test_cases); ++i) {
    const TestCase test_case = test_cases[i];
    SCOPED_TRACE(base::StringPrintf("Testing case number %zu with url %s",
                                    i + 1, test_case.url.c_str()));

    WebRequestInfoInitParams params = GetRequestParamsForURL(test_case.url);
    ASSERT_TRUE(params.url.is_valid());
    params.type = test_case.type;

    if (test_case.initiator)
      params.initiator = url::Origin::Create(GURL(*test_case.initiator));

    params.frame_id = test_case.frame_routing_id;

    if (test_case.frame_data_params) {
      const FrameDataParams& frame_params = *test_case.frame_data_params;
      params.frame_data = ExtensionApiFrameIdMap::FrameData(
          frame_params.frame_id, frame_params.parent_frame_id, kDummyTabId,
          kDummyWindowId, GURL(frame_params.last_committed_main_frame_url),
          frame_params.pending_main_frame_url);
    }

    WebRequestInfo request_info(std::move(params));
    const std::vector<RequestAction>& actions = manager()->EvaluateRequest(
        request_info, false /*is_incognito_context*/);

    if (test_case.expect_blocked_with_allowed_pages) {
      ASSERT_EQ(1u, actions.size());
      EXPECT_EQ(
          RequestAction(RequestActionType::BLOCK, *test_case.matched_rule_id,
                        kDefaultPriority, dnr_api::SOURCE_TYPE_MANIFEST,
                        last_loaded_extension()->id()),
          actions[0]);
    } else {
      EXPECT_TRUE(actions.empty());
    }
  }
}

TEST_P(RulesetManagerTest, HostPermissionForInitiator) {
  // Add an extension which redirects all sub-resource and sub-frame requests
  // made to example.com, to foo.com. By default, the "main_frame" type is
  // excluded if no "resource_types" are specified.
  std::unique_ptr<CompositeMatcher> redirect_matcher;
  {
    TestRule rule = CreateGenericRule();
    rule.id = kMinValidID;
    rule.priority = kMinValidPriority;
    rule.condition->url_filter = std::string("example.com");
    rule.action->type = std::string("redirect");
    rule.action->redirect.emplace();
    rule.action->redirect->url = std::string("https://foo.com");
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
  std::unique_ptr<CompositeMatcher> blocking_matcher;
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
    bool expected_action_redirect_extension;
    bool expected_action_blocking_extension;
  } cases[] = {
      // Empty initiator. Has access.
      {"https://example.com", base::nullopt, true, true},
      // Opaque origin as initiator. Has access.
      {"https://example.com", url::Origin(), true, true},
      // yahoo.com as initiator. Has access.
      {"https://example.com", url::Origin::Create(GURL("http://yahoo.com")),
       true, true},
      // No matching rule.
      {"https://yahoo.com", url::Origin::Create(GURL("http://example.com")),
       false, false},
      // Doesn't have access to initiator. But blocking a request doesn't
      // require host permissions.
      {"https://example.com", url::Origin::Create(GURL("http://google.com")),
       false, true},
  };

  auto verify_test_case = [this](const std::string& url,
                                 const base::Optional<url::Origin>& initiator,
                                 RequestAction* expected_action) {
    SCOPED_TRACE(base::StringPrintf(
        "Url-%s initiator-%s", url.c_str(),
        initiator ? initiator->Serialize().c_str() : "empty"));

    WebRequestInfo request(GetRequestParamsForURL(url, initiator));

    bool is_incognito_context = false;
    manager()->EvaluateRequest(request, is_incognito_context);

    if (expected_action) {
      ASSERT_EQ(1u, request.dnr_actions->size());
      EXPECT_EQ(*expected_action, (*request.dnr_actions)[0]);
    } else {
      EXPECT_TRUE(request.dnr_actions->empty());
    }
  };

  // Test redirect extension.
  {
    SCOPED_TRACE("Testing redirect extension");
    manager()->AddRuleset(redirect_extension_id, std::move(redirect_matcher),
                          URLPatternSet());
    for (const auto& test : cases) {
      RequestAction redirect_action(
          RequestActionType::REDIRECT, kMinValidID, kMinValidPriority,
          dnr_api::SOURCE_TYPE_MANIFEST, redirect_extension_id);
      redirect_action.redirect_url = GURL("https://foo.com/");

      verify_test_case(
          test.url, test.initiator,
          test.expected_action_redirect_extension ? &redirect_action : nullptr);
    }
    manager()->RemoveRuleset(redirect_extension_id);
  }

  // Test blocking extension.
  {
    SCOPED_TRACE("Testing blocking extension");
    manager()->AddRuleset(blocking_extension_id, std::move(blocking_matcher),
                          URLPatternSet());
    for (const auto& test : cases) {
      RequestAction block_action(
          RequestActionType::BLOCK, kMinValidID, kDefaultPriority,
          dnr_api::SOURCE_TYPE_MANIFEST, blocking_extension_id);

      verify_test_case(
          test.url, test.initiator,
          test.expected_action_blocking_extension ? &block_action : nullptr);
    }
    manager()->RemoveRuleset(blocking_extension_id);
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         RulesetManagerTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
