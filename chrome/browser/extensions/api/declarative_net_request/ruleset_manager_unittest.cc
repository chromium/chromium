// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/declarative_net_request/ruleset_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/extensions/api/declarative_net_request/dnr_test_base.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/extensions/extension_util.h"
#include "extensions/browser/api/declarative_net_request/composite_matcher.h"
#include "extensions/browser/api/declarative_net_request/file_backed_ruleset_source.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/browser/api/declarative_net_request/ruleset_matcher.h"
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
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {
namespace declarative_net_request {

namespace dnr_api = api::declarative_net_request;

namespace {

class RulesetManagerTest : public DNRTestBase {
 public:
  RulesetManagerTest() {}

  RulesetManagerTest(const RulesetManagerTest&) = delete;
  RulesetManagerTest& operator=(const RulesetManagerTest&) = delete;

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
    ConfigFlag flags = has_background_script
                           ? ConfigFlag::kConfig_HasBackgroundScript
                           : ConfigFlag::kConfig_None;

    constexpr char kRulesetID[] = "id";
    constexpr char kJSONRulesFilename[] = "rules_file.json";
    TestRulesetInfo info(kRulesetID, kJSONRulesFilename, ToListValue(rules));
    WriteManifestAndRuleset(extension_dir, info, host_permissions, flags);

    last_loaded_extension_ =
        CreateExtensionLoader()->LoadExtension(extension_dir);
    ASSERT_TRUE(last_loaded_extension_);

    ExtensionRegistry::Get(browser_context())
        ->AddEnabled(last_loaded_extension_);

    std::vector<FileBackedRulesetSource> sources =
        FileBackedRulesetSource::CreateStatic(
            *last_loaded_extension_,
            FileBackedRulesetSource::RulesetFilter::kIncludeAll);
    ASSERT_EQ(1u, sources.size());

    int expected_checksum;
    EXPECT_TRUE(ExtensionPrefs::Get(browser_context())
                    ->GetDNRStaticRulesetChecksum(last_loaded_extension_->id(),
                                                  sources[0].id(),
                                                  &expected_checksum));

    std::vector<std::unique_ptr<RulesetMatcher>> matchers(1);
    EXPECT_EQ(
        LoadRulesetResult::kSuccess,
        sources[0].CreateVerifiedMatcher(expected_checksum, &matchers[0]));

    *matcher = std::make_unique<CompositeMatcher>(
        std::move(matchers), HostPermissionsAlwaysRequired::kFalse);
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
      absl::optional<url::Origin> initiator = absl::nullopt) {
    const int kRendererId = 1;
    WebRequestInfoInitParams info;
    info.url = GURL(url);
    info.method = net::HttpRequestHeaders::kGetMethod;
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
    info.method = net::HttpRequestHeaders::kGetMethod;
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
            CreateRequestActionForTesting(
                RequestActionType::BLOCK, rule_id, kDefaultPriority,
                kMinValidStaticRulesetID, extension_id));
  };

  for (int mask = 0; mask < 4; mask++) {
    SCOPED_TRACE(base::StringPrintf("Testing ruleset mask %d", mask));

    ASSERT_EQ(0u, manager()->GetMatcherCountForTest());

    ExtensionId extension_id_one;
    ExtensionId extension_id_two;
    size_t expected_matcher_count = 0;

    // Add the required rulesets.
    if (mask & kEnableRulesetOne) {
      ++expected_matcher_count;
      std::unique_ptr<CompositeMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_one}, std::to_string(mask) + "_one", &matcher));
      extension_id_one = last_loaded_extension()->id();
      manager()->AddRuleset(extension_id_one, std::move(matcher));
    }
    if (mask & kEnableRulesetTwo) {
      ++expected_matcher_count;
      std::unique_ptr<CompositeMatcher> matcher;
      ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
          {rule_two}, std::to_string(mask) + "_two", &matcher));
      extension_id_two = last_loaded_extension()->id();
      manager()->AddRuleset(extension_id_two, std::move(matcher));
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
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher));

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
  EXPECT_EQ(CreateRequestActionForTesting(
                RequestActionType::BLOCK, *rule_one.id, kDefaultPriority,
                kMinValidStaticRulesetID, last_loaded_extension()->id()),
            (*request_info.dnr_actions)[0]);
  request_info.dnr_actions.reset();

  // Enabling the extension in incognito mode, should cause requests from
  // incognito contexts to also be evaluated.
  SetIncognitoEnabled(last_loaded_extension(), true /*incognito_enabled*/);
  EXPECT_TRUE(util::IsIncognitoEnabled(last_loaded_extension()->id(),
                                       browser_context()));

  manager()->EvaluateRequest(request_info, true /*is_incognito_context*/);
  ASSERT_EQ(1u, request_info.dnr_actions->size());
  EXPECT_EQ(CreateRequestActionForTesting(
                RequestActionType::BLOCK, *rule_one.id, kDefaultPriority,
                kMinValidStaticRulesetID, last_loaded_extension()->id()),
            (*request_info.dnr_actions)[0]);
  request_info.dnr_actions.reset();

  manager()->EvaluateRequest(request_info, false /*is_incognito_context*/);
  ASSERT_EQ(1u, request_info.dnr_actions->size());
  EXPECT_EQ(CreateRequestActionForTesting(
                RequestActionType::BLOCK, *rule_one.id, kDefaultPriority,
                kMinValidStaticRulesetID, last_loaded_extension()->id()),
            (*request_info.dnr_actions)[0]);
  request_info.dnr_actions.reset();
}

// Tests that
// Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions3
// is only emitted when there are active rulesets.
TEST_P(RulesetManagerTest, TotalEvaluationTimeHistogram) {
  WebRequestInfo example_com_request(
      GetRequestParamsForURL("http://example.com"));
  WebRequestInfo google_com_request(
      GetRequestParamsForURL("http://google.com"));
  bool is_incognito_context = false;
  const char* kHistogramName =
      "Extensions.DeclarativeNetRequest.EvaluateRequestTime.AllExtensions3";
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
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher));

  {
    base::HistogramTester tester;

    manager()->EvaluateRequest(example_com_request, is_incognito_context);
    ASSERT_EQ(1u, example_com_request.dnr_actions->size());
    EXPECT_EQ(CreateRequestActionForTesting(
                  RequestActionType::BLOCK, *rule.id, kDefaultPriority,
                  kMinValidStaticRulesetID, last_loaded_extension()->id()),
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
  manager()->AddRuleset(last_loaded_extension()->id(), std::move(matcher));

  // Create a request to "example.com" with an empty initiator. It should be
  // redirected to "google.com".
  const bool is_incognito_context = false;
  const char* kExampleURL = "http://example.com";
  RequestAction expected_redirect_action = CreateRequestActionForTesting(
      RequestActionType::REDIRECT, *rule.id, *rule.priority,
      kMinValidStaticRulesetID, last_loaded_extension()->id());
  expected_redirect_action.redirect_url = GURL("http://google.com");
  WebRequestInfo request_1(GetRequestParamsForURL(kExampleURL, absl::nullopt));
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
      GetRequestParamsForURL("ws://example.com", absl::nullopt));
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
        true /* has_background_script */));
    extension_1 = last_loaded_extension();
    manager()->AddRuleset(extension_1->id(), std::move(matcher));
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
        true /* has_background_script */));
    extension_2 = last_loaded_extension();
    manager()->AddRuleset(extension_2->id(), std::move(matcher));
  }

  EXPECT_EQ(2u, manager()->GetMatcherCountForTest());

  // Ensure that "http://example.com" will be blocked (with blocking taking
  // priority over redirection).
  WebRequestInfo request_1(GetRequestParamsForURL("http://example.com"));
  manager()->EvaluateRequest(request_1, false /*is_incognito_context*/);
  ASSERT_EQ(1u, request_1.dnr_actions->size());
  EXPECT_EQ(CreateRequestActionForTesting(
                RequestActionType::BLOCK, kMinValidID, kDefaultPriority,
                kMinValidStaticRulesetID, extension_1->id()),
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

// Test that the correct modifyHeaders actions are returned for each extension.
TEST_P(RulesetManagerTest, ModifyHeaders) {
  const Extension* extension_1 = nullptr;
  const Extension* extension_2 = nullptr;

  // Add an extension which removes "header1" and sets "header2".
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.action->type = std::string("modifyHeaders");
    rule.action->request_headers = std::vector<TestHeaderInfo>(
        {TestHeaderInfo("header1", "remove", absl::nullopt),
         TestHeaderInfo("header2", "set", "value2")});

    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension", &matcher, {URLPattern::kAllUrlsPattern}));
    extension_1 = last_loaded_extension();
    manager()->AddRuleset(extension_1->id(), std::move(matcher));
  }

  // Add another extension which removes "header1" and appends "header3".
  {
    std::unique_ptr<CompositeMatcher> matcher;
    TestRule rule = CreateGenericRule();
    rule.condition->url_filter = std::string("*");
    rule.action->type = std::string("modifyHeaders");
    rule.action->request_headers = std::vector<TestHeaderInfo>(
        {TestHeaderInfo("header1", "remove", absl::nullopt)});
    rule.action->response_headers = std::vector<TestHeaderInfo>(
        {TestHeaderInfo("header3", "append", "value3")});

    ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
        {rule}, "test extension 2", &matcher, {URLPattern::kAllUrlsPattern}));
    extension_2 = last_loaded_extension();
    manager()->AddRuleset(extension_2->id(), std::move(matcher));
  }

  ASSERT_EQ(2u, manager()->GetMatcherCountForTest());

  WebRequestInfo request(GetRequestParamsForURL("http://example.com"));

  const std::vector<RequestAction>& actual_actions =
      manager()->EvaluateRequest(request, false /*is_incognito_context*/);

  // Create the expected RequestAction for |extension_2|.
  RequestAction expected_action_1 = CreateRequestActionForTesting(
      RequestActionType::MODIFY_HEADERS, kMinValidID, kDefaultPriority,
      kMinValidStaticRulesetID, extension_2->id());
  expected_action_1.request_headers_to_modify = {RequestAction::HeaderInfo(
      "header1", dnr_api::HeaderOperation::kRemove, absl::nullopt)};
  expected_action_1.response_headers_to_modify = {RequestAction::HeaderInfo(
      "header3", dnr_api::HeaderOperation::kAppend, "value3")};

  // Create the expected RequestAction for |extension_1|.
  RequestAction expected_action_2 = CreateRequestActionForTesting(
      RequestActionType::MODIFY_HEADERS, kMinValidID, kDefaultPriority,
      kMinValidStaticRulesetID, extension_1->id());
  expected_action_2.request_headers_to_modify = {
      RequestAction::HeaderInfo("header1", dnr_api::HeaderOperation::kRemove,
                                absl::nullopt),
      RequestAction::HeaderInfo("header2", dnr_api::HeaderOperation::kSet,
                                "value2")};

  // Verify that the list of actions is sorted in descending order of extension
  // priority.
  EXPECT_THAT(actual_actions,
              ::testing::ElementsAre(
                  ::testing::Eq(::testing::ByRef(expected_action_1)),
                  ::testing::Eq(::testing::ByRef(expected_action_2))));
}

// Ensures that an allow rule doesn't win over a higher priority modifyHeaders
// rule. Regression test for crbug.com/1244249.
TEST_P(RulesetManagerTest, ModifyHeadersWithAllowRules) {
  int rule_id = kMinValidID;

  TestRule rule1 = CreateGenericRule();
  rule1.condition->url_filter = std::string("example.com");
  rule1.action->type = std::string("modifyHeaders");
  rule1.action->request_headers =
      std::vector<TestHeaderInfo>({TestHeaderInfo("header", "set", "value")});
  rule1.priority = 5;
  rule1.id = rule_id++;

  TestRule rule2 = CreateGenericRule();
  rule2.condition->url_filter = std::string("example.com");
  rule2.action->type = std::string("allow");
  rule2.priority = 3;
  rule2.id = rule_id++;

  TestRule rule3 = CreateGenericRule();
  rule3.condition->url_filter = std::string("abc.example.com");
  rule3.action->type = std::string("allow");
  rule3.priority = 8;
  rule3.id = rule_id++;

  std::unique_ptr<CompositeMatcher> matcher;
  ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules({rule1, rule2, rule3},
                                                "test extension", &matcher,
                                                {URLPattern::kAllUrlsPattern}));
  const Extension* extension = last_loaded_extension();
  manager()->AddRuleset(extension->id(), std::move(matcher));

  {
    WebRequestInfo request(GetRequestParamsForURL("http://example.com"));
    const std::vector<RequestAction>& actions =
        manager()->EvaluateRequest(request, false /*is_incognito_context*/);

    // `rule1` and `rule2` match. `rule1` gets precedence because of priority.
    RequestAction expected_action = CreateRequestActionForTesting(
        RequestActionType::MODIFY_HEADERS, *rule1.id, *rule1.priority,
        kMinValidStaticRulesetID, extension->id());
    expected_action.request_headers_to_modify = {RequestAction::HeaderInfo(
        "header", dnr_api::HeaderOperation::kSet, "value")};
    EXPECT_THAT(actions, ::testing::ElementsAre(
                             ::testing::Eq(::testing::ByRef(expected_action))));
  }

  {
    WebRequestInfo request(GetRequestParamsForURL("http://abc.example.com"));
    const std::vector<RequestAction>& actions =
        manager()->EvaluateRequest(request, false /*is_incognito_context*/);

    // `rule1`, `rule2` and `rule3` match. `rule3` gets precedence because of
    // priority.
    RequestAction expected_action = CreateRequestActionForTesting(
        RequestActionType::ALLOW, *rule3.id, *rule3.priority,
        kMinValidStaticRulesetID, extension->id());
    EXPECT_THAT(actions, ::testing::ElementsAre(
                             ::testing::Eq(::testing::ByRef(expected_action))));
  }
}

// Test that an extension's modify header rules are applied on a request only if
// it has host permissions for the request.
TEST_P(RulesetManagerTest, ModifyHeaders_HostPermissions) {
  // Add an extension which removes the "header1" header with host permissions
  // for example.com.
  std::unique_ptr<CompositeMatcher> matcher;
  TestRule rule = CreateGenericRule();
  rule.condition->url_filter = std::string("*");
  rule.action->type = std::string("modifyHeaders");
  rule.action->request_headers = std::vector<TestHeaderInfo>(
      {TestHeaderInfo("header1", "remove", absl::nullopt)});

  ASSERT_NO_FATAL_FAILURE(CreateMatcherForRules(
      {rule}, "test extension", &matcher, {"*://example.com/*"}));
  const Extension* extension = last_loaded_extension();
  manager()->AddRuleset(extension->id(), std::move(matcher));

  ASSERT_EQ(1u, manager()->GetMatcherCountForTest());

  {
    WebRequestInfo request(GetRequestParamsForURL("http://example.com"));
    const std::vector<RequestAction>& actual_actions =
        manager()->EvaluateRequest(request, false /*is_incognito_context*/);

    RequestAction expected_action = CreateRequestActionForTesting(
        RequestActionType::MODIFY_HEADERS, kMinValidID, kDefaultPriority,
        kMinValidStaticRulesetID, extension->id());
    expected_action.request_headers_to_modify = {RequestAction::HeaderInfo(
        "header1", dnr_api::HeaderOperation::kRemove, absl::nullopt)};

    EXPECT_THAT(actual_actions, ::testing::ElementsAre(::testing::Eq(
                                    ::testing::ByRef(expected_action))));
  }

  {
    WebRequestInfo request(GetRequestParamsForURL("http://nopermissions.com"));
    const std::vector<RequestAction>& actual_actions =
        manager()->EvaluateRequest(request, false /*is_incognito_context*/);
    EXPECT_TRUE(actual_actions.empty());
  }

  {
    // Has access to url but no access to initiator, so no actions should be
    // returned.
    WebRequestInfo request(GetRequestParamsForURL(
        "http://example.com",
        url::Origin::Create(GURL("http://nopermissions.com"))));
    const std::vector<RequestAction>& actual_actions =
        manager()->EvaluateRequest(request, false /*is_incognito_context*/);
    EXPECT_TRUE(actual_actions.empty());
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
    absl::optional<url::Origin> initiator;
    bool expected_action_redirect_extension;
    bool expected_action_blocking_extension;
  } cases[] = {
      // Empty initiator. Has access.
      {"https://example.com", absl::nullopt, true, true},
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
                                 const absl::optional<url::Origin>& initiator,
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
    manager()->AddRuleset(redirect_extension_id, std::move(redirect_matcher));
    for (const auto& test : cases) {
      RequestAction redirect_action = CreateRequestActionForTesting(
          RequestActionType::REDIRECT, kMinValidID, kMinValidPriority,
          kMinValidStaticRulesetID, redirect_extension_id);
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
    manager()->AddRuleset(blocking_extension_id, std::move(blocking_matcher));
    for (const auto& test : cases) {
      RequestAction block_action = CreateRequestActionForTesting(
          RequestActionType::BLOCK, kMinValidID, kDefaultPriority,
          kMinValidStaticRulesetID, blocking_extension_id);

      verify_test_case(
          test.url, test.initiator,
          test.expected_action_blocking_extension ? &block_action : nullptr);
    }
    manager()->RemoveRuleset(blocking_extension_id);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         RulesetManagerTest,
                         ::testing::Values(ExtensionLoadType::PACKED,
                                           ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
