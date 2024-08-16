// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/declarative_webrequest/webrequest_rules_registry.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/run_loop.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "components/url_matcher/url_matcher_constants.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/api/declarative/rules_registry_service.h"
#include "extensions/browser/api/declarative_webrequest/webrequest_constants.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest-message.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace helpers = extension_web_request_api_helpers;
namespace keys = extensions::declarative_webrequest_constants;
namespace keys2 = url_matcher::url_matcher_constants;

using base::Value;
using extension_test_util::LoadManifest;
using extension_test_util::LoadManifestUnchecked;
using helpers::EventResponseDelta;
using helpers::EventResponseDeltas;
using testing::HasSubstr;
using url_matcher::URLMatcher;

namespace extensions {

namespace {
const char kExtensionId[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const char kExtensionId2[] = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";
const char kRuleId1[] = "rule1";
const char kRuleId2[] = "rule2";
const char kRuleId3[] = "rule3";
const char kRuleId4[] = "rule4";

// Creates a main-frame navigation request to |url|.
WebRequestInfoInitParams CreateRequestParams(const GURL& url) {
  WebRequestInfoInitParams info;
  info.url = url;
  info.is_navigation_request = true;
  info.web_request_type = WebRequestResourceType::MAIN_FRAME;
  return info;
}

}  // namespace

class TestWebRequestRulesRegistry : public WebRequestRulesRegistry {
 public:
  explicit TestWebRequestRulesRegistry(content::BrowserContext* context)
      : WebRequestRulesRegistry(context,
                                nullptr /* cache_delegate */,
                                RulesRegistryService::kDefaultRulesRegistryID),
        num_clear_cache_calls_(0) {}

  // Returns how often the in-memory caches of the renderers were instructed
  // to be cleared.
  int num_clear_cache_calls() const { return num_clear_cache_calls_; }

  // How many rules are there which have some conditions not triggered by URL
  // matches.
  size_t RulesWithoutTriggers() const {
    return rules_with_untriggered_conditions_for_test().size();
  }

 protected:
  ~TestWebRequestRulesRegistry() override {}

  void ClearCacheOnNavigation() override { ++num_clear_cache_calls_; }

 private:
  int num_clear_cache_calls_;
};

class WebRequestRulesRegistryTest : public testing::Test {
 public:
  void SetUp() override;

  void TearDown() override {
    // Make sure that deletion traits of all registries are executed.
    base::RunLoop().RunUntilIdle();
  }

  // Returns a rule that roughly matches http://*.example.com and
  // https://www.example.com and cancels it
  api::events::Rule CreateRule1() {
    base::Value::List scheme_http;
    scheme_http.Append("http");
    base::Value::Dict http_condition_dict;
    http_condition_dict.Set(keys2::kHostSuffixKey, "example.com");
    base::Value::Dict http_condition_url_filter;
    http_condition_url_filter.Set(keys::kInstanceTypeKey,
                                  keys::kRequestMatcherType);

    scheme_http.Append("https");
    base::Value::Dict https_condition_dict;
    https_condition_dict.Set(keys2::kSchemesKey, base::Value::List());
    https_condition_dict.Set(keys2::kHostSuffixKey, "example.com");
    https_condition_dict.Set(keys2::kHostPrefixKey, "www");

    base::Value::Dict https_condition_url_filter;
    https_condition_url_filter.Set(keys::kUrlKey,
                                   std::move(https_condition_dict));
    https_condition_url_filter.Set(keys::kInstanceTypeKey,
                                   keys::kRequestMatcherType);

    base::Value::Dict action_dict;
    action_dict.Set(keys::kInstanceTypeKey, keys::kCancelRequestType);

    api::events::Rule rule;
    rule.id = kRuleId1;
    rule.priority = 100;
    rule.actions.Append(std::move(action_dict));
    http_condition_dict.Set(keys2::kSchemesKey, std::move(scheme_http));
    http_condition_url_filter.Set(keys::kUrlKey,
                                  std::move(http_condition_dict));
    rule.conditions.Append(std::move(http_condition_url_filter));
    rule.conditions.Append(std::move(https_condition_url_filter));
    return rule;
  }

  // Returns a rule that matches anything and cancels it.
  api::events::Rule CreateRule2() {
    base::Value::Dict condition_dict;
    condition_dict.Set(keys::kInstanceTypeKey, keys::kRequestMatcherType);

    base::Value::Dict action_dict;
    action_dict.Set(keys::kInstanceTypeKey, keys::kCancelRequestType);

    api::events::Rule rule;
    rule.id = kRuleId2;
    rule.priority = 100;
    rule.actions.Append(std::move(action_dict));
    rule.conditions.Append(std::move(condition_dict));
    return rule;
  }

  api::events::Rule CreateRedirectRule(const std::string& destination) {
    base::Value::Dict condition_dict;
    condition_dict.Set(keys::kInstanceTypeKey, keys::kRequestMatcherType);

    base::Value::Dict action_dict;
    action_dict.Set(keys::kInstanceTypeKey, keys::kRedirectRequestType);
    action_dict.Set(keys::kRedirectUrlKey, destination);

    api::events::Rule rule;
    rule.id = kRuleId3;
    rule.priority = 100;
    rule.actions.Append(std::move(action_dict));
    rule.conditions.Append(std::move(condition_dict));
    return rule;
  }

  // Create a rule to ignore all other rules for a destination that
  // contains index.html.
  api::events::Rule CreateIgnoreRule() {
    base::Value::Dict condition_dict;
    base::Value::Dict http_condition_dict;
    http_condition_dict.Set(keys2::kPathContainsKey, "index.html");
    condition_dict.Set(keys::kInstanceTypeKey, keys::kRequestMatcherType);
    condition_dict.Set(keys::kUrlKey, std::move(http_condition_dict));

    base::Value::Dict action_dict;
    action_dict.Set(keys::kInstanceTypeKey, keys::kIgnoreRulesType);
    action_dict.Set(keys::kLowerPriorityThanKey, 150);

    api::events::Rule rule;
    rule.id = kRuleId4;
    rule.priority = 200;
    rule.actions.Append(std::move(action_dict));
    rule.conditions.Append(std::move(condition_dict));
    return rule;
  }

  // Create a condition with the attributes specified. An example value of
  // |attributes| is: "\"resourceType\": [\"stylesheet\"], \n".
  base::Value CreateCondition(const std::string& attributes) {
    std::string json_description =
        "{ \n"
        "  \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n";
    json_description += attributes;
    json_description += "}";

    return base::test::ParseJson(json_description);
  }

  // Create a rule with the ID |rule_id| and with conditions created from the
  // |attributes| specified (one entry one condition). An example value of a
  // string from |attributes| is: "\"resourceType\": [\"stylesheet\"], \n".
  api::events::Rule CreateCancellingRule(
      const char* rule_id,
      const std::vector<const std::string*>& attributes) {
    base::Value::Dict action_dict;
    action_dict.Set(keys::kInstanceTypeKey, keys::kCancelRequestType);

    api::events::Rule rule;
    rule.id = rule_id;
    rule.priority = 1;
    rule.actions.Append(std::move(action_dict));
    for (auto* attribute : attributes)
      rule.conditions.Append(CreateCondition(*attribute));
    return rule;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::ScopedLacrosServiceTestHelper lacros_service_test_helper_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  TestingProfile profile_;
  // Two extensions with host permissions for all URLs and the DWR permission.
  // Installation times will be so that |extension_| is older than
  // |extension2_|.
  scoped_refptr<Extension> extension_;
  scoped_refptr<Extension> extension2_;
};

void WebRequestRulesRegistryTest::SetUp() {
  testing::Test::SetUp();

  std::string error;
  extension_ = LoadManifestUnchecked("permissions",
                                     "web_request_all_host_permissions.json",
                                     mojom::ManifestLocation::kInvalidLocation,
                                     Extension::NO_FLAGS, kExtensionId, &error);
  ASSERT_TRUE(extension_.get()) << error;
  extension2_ = LoadManifestUnchecked(
      "permissions", "web_request_all_host_permissions.json",
      mojom::ManifestLocation::kInvalidLocation, Extension::NO_FLAGS,
      kExtensionId2, &error);
  ASSERT_TRUE(extension2_.get()) << error;
  CHECK(ExtensionRegistry::Get(&profile_));
  ExtensionRegistry::Get(&profile_)->AddEnabled(extension_);
  ExtensionPrefs::Get(&profile_)->OnExtensionInstalled(
      extension_.get(), Extension::State::ENABLED, syncer::StringOrdinal(), "");
  ExtensionRegistry::Get(&profile_)->AddEnabled(extension2_);
  ExtensionPrefs::Get(&profile_)->OnExtensionInstalled(
      extension2_.get(), Extension::State::ENABLED, syncer::StringOrdinal(),
      "");
}


TEST_F(WebRequestRulesRegistryTest, AddRulesImpl) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  std::string error;

  {
    std::vector<api::events::Rule> rules;
    rules.push_back(CreateRule1());
    rules.push_back(CreateRule2());

    error = registry->AddRules(kExtensionId, std::move(rules));
    EXPECT_EQ("", error);
    EXPECT_EQ(1, registry->num_clear_cache_calls());
  }

  std::set<const WebRequestRule*> matches;

  GURL http_url("http://www.example.com");
  WebRequestInfo http_request_info(CreateRequestParams(http_url));
  WebRequestData request_data(&http_request_info, ON_BEFORE_REQUEST);
  matches = registry->GetMatches(request_data);
  EXPECT_EQ(2u, matches.size());

  std::set<WebRequestRule::GlobalRuleId> matches_ids;
  for (const auto* match : matches)
    matches_ids.insert(match->id());
  EXPECT_TRUE(
      base::Contains(matches_ids, std::make_pair(kExtensionId, kRuleId1)));
  EXPECT_TRUE(
      base::Contains(matches_ids, std::make_pair(kExtensionId, kRuleId2)));

  GURL foobar_url("http://www.foobar.com");
  WebRequestInfo foobar_request_info(CreateRequestParams(foobar_url));
  request_data.request = &foobar_request_info;
  matches = registry->GetMatches(request_data);
  EXPECT_EQ(1u, matches.size());
  WebRequestRule::GlobalRuleId expected_pair =
      std::make_pair(kExtensionId, kRuleId2);
  EXPECT_EQ(expected_pair, (*matches.begin())->id());
}

TEST_F(WebRequestRulesRegistryTest, RemoveRulesImpl) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  std::string error;

  // Setup RulesRegistry to contain two rules.
  {
    std::vector<api::events::Rule> rules_to_add;
    rules_to_add.push_back(CreateRule1());
    rules_to_add.push_back(CreateRule2());
    error = registry->AddRules(kExtensionId, std::move(rules_to_add));
    EXPECT_EQ("", error);
    EXPECT_EQ(1, registry->num_clear_cache_calls());
  }

  // Verify initial state.
  std::vector<const api::events::Rule*> registered_rules;
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(2u, registered_rules.size());
  EXPECT_EQ(1u, registry->RulesWithoutTriggers());

  // Remove first rule.
  std::vector<std::string> rules_to_remove;
  rules_to_remove.push_back(kRuleId1);
  error = registry->RemoveRules(kExtensionId, rules_to_remove);
  EXPECT_EQ("", error);
  EXPECT_EQ(2, registry->num_clear_cache_calls());

  // Verify that only one rule is left.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());
  EXPECT_EQ(1u, registry->RulesWithoutTriggers());

  // Now rules_to_remove contains both rules, i.e. one that does not exist in
  // the rules registry anymore. Effectively we only remove the second rule.
  rules_to_remove.push_back(kRuleId2);
  error = registry->RemoveRules(kExtensionId, rules_to_remove);
  EXPECT_EQ("", error);
  EXPECT_EQ(3, registry->num_clear_cache_calls());

  // Verify that everything is gone.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(0u, registered_rules.size());
  EXPECT_EQ(0u, registry->RulesWithoutTriggers());

  EXPECT_TRUE(registry->IsEmpty());
}

TEST_F(WebRequestRulesRegistryTest, RemoveAllRulesImpl) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  std::string error;

  {
    // Setup RulesRegistry to contain two rules, one for each extension.
    std::vector<api::events::Rule> rules_to_add;
    rules_to_add.push_back(CreateRule1());
    error = registry->AddRules(kExtensionId, std::move(rules_to_add));
    EXPECT_EQ("", error);
    EXPECT_EQ(1, registry->num_clear_cache_calls());
  }

  {
    std::vector<api::events::Rule> rules_to_add;
    rules_to_add.push_back(CreateRule2());
    error = registry->AddRules(kExtensionId2, std::move(rules_to_add));
    EXPECT_EQ("", error);
    EXPECT_EQ(2, registry->num_clear_cache_calls());
  }

  // Verify initial state.
  std::vector<const api::events::Rule*> registered_rules;
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());
  registered_rules.clear();
  registry->GetAllRules(kExtensionId2, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());

  // Remove rule of first extension.
  error = registry->RemoveAllRules(kExtensionId);
  EXPECT_EQ("", error);
  EXPECT_EQ(3, registry->num_clear_cache_calls());

  // Verify that only the first rule is deleted.
  registered_rules.clear();
  registry->GetAllRules(kExtensionId, &registered_rules);
  EXPECT_EQ(0u, registered_rules.size());
  registered_rules.clear();
  registry->GetAllRules(kExtensionId2, &registered_rules);
  EXPECT_EQ(1u, registered_rules.size());

  // Test removing rules if none exist.
  error = registry->RemoveAllRules(kExtensionId);
  EXPECT_EQ("", error);
  EXPECT_EQ(4, registry->num_clear_cache_calls());

  // Remove rule from second extension.
  error = registry->RemoveAllRules(kExtensionId2);
  EXPECT_EQ("", error);
  EXPECT_EQ(5, registry->num_clear_cache_calls());

  EXPECT_TRUE(registry->IsEmpty());
}

// Test precedences between extensions.
TEST_F(WebRequestRulesRegistryTest, Precedences) {
  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  std::string error;

  {
    std::vector<api::events::Rule> rules_to_add_1(1);
    rules_to_add_1[0] = CreateRedirectRule("http://www.foo.com");
    error = registry->AddRules(kExtensionId, std::move(rules_to_add_1));
    EXPECT_EQ("", error);
  }

  {
    std::vector<api::events::Rule> rules_to_add_2(1);
    rules_to_add_2[0] = CreateRedirectRule("http://www.bar.com");
    error = registry->AddRules(kExtensionId2, std::move(rules_to_add_2));
    EXPECT_EQ("", error);
  }

  GURL url("http://www.google.com");
  WebRequestInfo request_info(CreateRequestParams(url));
  WebRequestData request_data(&request_info, ON_BEFORE_REQUEST);
  EventResponseDeltas deltas = registry->CreateDeltas(
      PermissionHelper::Get(&profile_), request_data, false);

  // The second extension is installed later and will win for this reason
  // in conflict resolution.
  ASSERT_EQ(2u, deltas.size());
  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  const EventResponseDelta& winner = deltas.front();
  const EventResponseDelta& loser = deltas.back();

  EXPECT_EQ(kExtensionId2, winner.extension_id);
  EXPECT_EQ(GURL("http://www.bar.com"), winner.new_url);
  EXPECT_GT(winner.extension_install_time, loser.extension_install_time);

  EXPECT_EQ(kExtensionId, loser.extension_id);
  EXPECT_EQ(GURL("http://www.foo.com"), loser.new_url);
}

// Test priorities of rules within one extension.
TEST_F(WebRequestRulesRegistryTest, Priorities) {
  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  std::string error;

  {
    std::vector<api::events::Rule> rules_to_add_1(1);
    rules_to_add_1[0] = CreateRedirectRule("http://www.foo.com");
    error = registry->AddRules(kExtensionId, std::move(rules_to_add_1));
    EXPECT_EQ("", error);
  }

  {
    std::vector<api::events::Rule> rules_to_add_2(1);
    rules_to_add_2[0] = CreateRedirectRule("http://www.bar.com");
    error = registry->AddRules(kExtensionId2, std::move(rules_to_add_2));
    EXPECT_EQ("", error);
  }

  {
    std::vector<api::events::Rule> rules_to_add_3(1);
    rules_to_add_3[0] = CreateIgnoreRule();
    error = registry->AddRules(kExtensionId, std::move(rules_to_add_3));
    EXPECT_EQ("", error);
  }

  GURL url("http://www.google.com/index.html");
  WebRequestInfo request_info(CreateRequestParams(url));
  WebRequestData request_data(&request_info, ON_BEFORE_REQUEST);
  EventResponseDeltas deltas = registry->CreateDeltas(
      PermissionHelper::Get(&profile_), request_data, false);

  // The redirect by the first extension is ignored due to the ignore rule.
  ASSERT_EQ(1u, deltas.size());
  const EventResponseDelta& effective_rule = *deltas.begin();

  EXPECT_EQ(kExtensionId2, effective_rule.extension_id);
  EXPECT_EQ(GURL("http://www.bar.com"), effective_rule.new_url);
}

// Test ignoring of rules by tag.
TEST_F(WebRequestRulesRegistryTest, IgnoreRulesByTag) {
  const char kRule1[] =
      "{                                                                 \n"
      "  \"id\": \"rule1\",                                              \n"
      "  \"tags\": [\"non_matching_tag\", \"ignore_tag\"],               \n"
      "  \"conditions\": [                                               \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "      \"url\": {\"hostSuffix\": \"foo.com\"}                      \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"actions\": [                                                  \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RedirectRequest\",\n"
      "      \"redirectUrl\": \"http://bar.com\"                         \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"priority\": 200                                               \n"
      "}                                                                 ";

  const char kRule2[] =
      "{                                                                 \n"
      "  \"id\": \"rule2\",                                              \n"
      "  \"conditions\": [                                               \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "      \"url\": {\"pathPrefix\": \"/test\"}                        \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"actions\": [                                                  \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.IgnoreRules\",    \n"
      "      \"hasTag\": \"ignore_tag\"                                  \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"priority\": 300                                               \n"
      "}                                                                 ";

  base::Value::Dict value1 = base::test::ParseJsonDict(kRule1);
  base::Value::Dict value2 = base::test::ParseJsonDict(kRule2);

  std::optional<api::events::Rule> rule1 = api::events::Rule::FromValue(value1);
  std::optional<api::events::Rule> rule2 = api::events::Rule::FromValue(value2);
  ASSERT_TRUE(rule1);
  ASSERT_TRUE(rule2);
  std::vector<const api::events::Rule*> rules = {&rule1.value(),
                                                 &rule2.value()};

  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  std::string error = registry->AddRulesImpl(kExtensionId, rules);
  EXPECT_EQ("", error);
  EXPECT_FALSE(registry->IsEmpty());

  GURL url("http://www.foo.com/test");
  WebRequestInfo request_info(CreateRequestParams(url));
  WebRequestData request_data(&request_info, ON_BEFORE_REQUEST);
  EventResponseDeltas deltas = registry->CreateDeltas(
      PermissionHelper::Get(&profile_), request_data, false);

  // The redirect by the redirect rule is ignored due to the ignore rule.
  std::set<const WebRequestRule*> matches = registry->GetMatches(request_data);
  EXPECT_EQ(2u, matches.size());
  ASSERT_EQ(0u, deltas.size());
}

// Test that rules failing IsFulfilled on their conditions are never returned by
// GetMatches.
TEST_F(WebRequestRulesRegistryTest, GetMatchesCheckFulfilled) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  const std::string kMatchingUrlAttribute(
      "\"url\": { \"pathContains\": \"\" }, \n");
  const std::string kNonMatchingNonUrlAttribute(
      "\"resourceType\": [\"stylesheet\"], \n");
  const std::string kBothAttributes(kMatchingUrlAttribute +
                                    kNonMatchingNonUrlAttribute);
  {
    std::string error;
    std::vector<const std::string*> attributes;
    std::vector<api::events::Rule> rules;

    // Rules 1 and 2 have one condition, neither of them should fire.
    attributes.push_back(&kNonMatchingNonUrlAttribute);
    rules.push_back(CreateCancellingRule(kRuleId1, attributes));

    attributes.clear();
    attributes.push_back(&kBothAttributes);
    rules.push_back(CreateCancellingRule(kRuleId2, attributes));

    // Rule 3 has two conditions, one with a matching URL attribute, and one
    // with a non-matching non-URL attribute.
    attributes.clear();
    attributes.push_back(&kMatchingUrlAttribute);
    attributes.push_back(&kNonMatchingNonUrlAttribute);
    rules.push_back(CreateCancellingRule(kRuleId3, attributes));

    error = registry->AddRules(kExtensionId, std::move(rules));
    EXPECT_EQ("", error);
    EXPECT_EQ(1, registry->num_clear_cache_calls());
  }

  std::set<const WebRequestRule*> matches;

  GURL http_url("http://www.example.com");
  WebRequestInfo http_request_info(CreateRequestParams(http_url));
  WebRequestData request_data(&http_request_info, ON_BEFORE_REQUEST);
  matches = registry->GetMatches(request_data);
  EXPECT_EQ(1u, matches.size());
  WebRequestRule::GlobalRuleId expected_pair = std::make_pair(kExtensionId,
                                                              kRuleId3);
  EXPECT_EQ(expected_pair, (*matches.begin())->id());
}

// Test different URL patterns.
TEST_F(WebRequestRulesRegistryTest, GetMatchesDifferentUrls) {
  scoped_refptr<TestWebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));
  const std::string kUrlAttribute(
      "\"url\": { \"hostContains\": \"url\" }, \n");
  const std::string kUrlAttribute2(
      "\"url\": { \"hostContains\": \"www\" }, \n");

  {
    std::string error;
    std::vector<const std::string*> attributes;
    std::vector<api::events::Rule> rules;

    // Rule 1 has one condition, with a url attribute
    attributes.push_back(&kUrlAttribute);
    rules.push_back(CreateCancellingRule(kRuleId1, attributes));

    // Rule 2 has one condition, also with a url attribute
    attributes.clear();
    attributes.push_back(&kUrlAttribute2);
    rules.push_back(CreateCancellingRule(kRuleId2, attributes));

    error = registry->AddRules(kExtensionId, std::move(rules));
    EXPECT_EQ("", error);
    EXPECT_EQ(1, registry->num_clear_cache_calls());
  }

  std::set<const WebRequestRule*> matches;

  const GURL urls[] = {
    GURL("http://url.example.com"),  // matching
    GURL("http://www.example.com")   // non-matching
  };
  // Which rules should match in subsequent test iterations.
  const char* const matchingRuleIds[] = { kRuleId1, kRuleId2 };
  static_assert(std::size(urls) == std::size(matchingRuleIds),
                "urls and matchingRuleIds must have the same number "
                "of elements");

  for (size_t i = 0; i < std::size(matchingRuleIds); ++i) {
    // Construct the inputs.
    WebRequestInfoInitParams params = CreateRequestParams(urls[i]);
    WebRequestInfo http_request_info(std::move(params));
    WebRequestData request_data(&http_request_info, ON_BEFORE_REQUEST);
    // Now run both rules on the input.
    matches = registry->GetMatches(request_data);
    SCOPED_TRACE(testing::Message("i = ") << i << ", rule id = "
                                          << matchingRuleIds[i]);
    // Make sure that the right rule succeeded.
    EXPECT_EQ(1u, matches.size());
    EXPECT_EQ(WebRequestRule::GlobalRuleId(std::make_pair(kExtensionId,
                                                          matchingRuleIds[i])),
              (*matches.begin())->id());
  }
}

TEST(WebRequestRulesRegistrySimpleTest, StageChecker) {
  // The contentType condition can only be evaluated during ON_HEADERS_RECEIVED
  // but the SetRequestHeader action can only be executed during
  // ON_BEFORE_SEND_HEADERS.
  // Therefore, this is an inconsistent rule that needs to be flagged.
  const char kRule[] =
      "{                                                                  \n"
      "  \"id\": \"rule1\",                                               \n"
      "  \"conditions\": [                                                \n"
      "    {                                                              \n"
      "      \"instanceType\": \"declarativeWebRequest.RequestMatcher\",  \n"
      "      \"url\": {\"hostSuffix\": \"foo.com\"},                      \n"
      "      \"contentType\": [\"image/jpeg\"]                            \n"
      "    }                                                              \n"
      "  ],                                                               \n"
      "  \"actions\": [                                                   \n"
      "    {                                                              \n"
      "      \"instanceType\": \"declarativeWebRequest.SetRequestHeader\",\n"
      "      \"name\": \"Content-Type\",                                  \n"
      "      \"value\": \"text/plain\"                                    \n"
      "    }                                                              \n"
      "  ],                                                               \n"
      "  \"priority\": 200                                                \n"
      "}                                                                  ";

  base::Value::Dict value = base::test::ParseJsonDict(kRule);

  std::optional<api::events::Rule> rule = api::events::Rule::FromValue(value);
  ASSERT_TRUE(rule);

  std::string error;
  URLMatcher matcher;
  std::unique_ptr<WebRequestConditionSet> conditions =
      WebRequestConditionSet::Create(nullptr, matcher.condition_factory(),
                                     rule->conditions, &error);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_TRUE(conditions);

  bool bad_message = false;
  std::unique_ptr<WebRequestActionSet> actions = WebRequestActionSet::Create(
      nullptr, nullptr, rule->actions, &error, &bad_message);
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_FALSE(bad_message);
  ASSERT_TRUE(actions);

  EXPECT_FALSE(WebRequestRulesRegistry::StageChecker(
      conditions.get(), actions.get(), &error));
  EXPECT_THAT(error, HasSubstr("no time in the request life-cycle"));
  EXPECT_THAT(error, HasSubstr(actions->actions().back()->GetName()));
}

TEST(WebRequestRulesRegistrySimpleTest, HostPermissionsChecker) {
  const char kAction[] =  // This action requires all URLs host permission.
      "{                                                             \n"
      "  \"instanceType\": \"declarativeWebRequest.RedirectRequest\",\n"
      "  \"redirectUrl\": \"http://bar.com\"                         \n"
      "}                                                             ";
  base::Value action_value = base::test::ParseJson(kAction);

  base::Value::List actions;
  actions.Append(std::move(action_value));

  std::string error;
  bool bad_message = false;
  std::unique_ptr<WebRequestActionSet> action_set(WebRequestActionSet::Create(
      nullptr, nullptr, actions, &error, &bad_message));
  ASSERT_TRUE(error.empty()) << error;
  ASSERT_FALSE(bad_message);
  ASSERT_TRUE(action_set);

  scoped_refptr<Extension> extension_no_url(
      LoadManifest("permissions", "web_request_no_host.json"));
  scoped_refptr<Extension> extension_some_urls(
      LoadManifest("permissions", "web_request_com_host_permissions.json"));
  scoped_refptr<Extension> extension_all_urls(
      LoadManifest("permissions", "web_request_all_host_permissions.json"));

  EXPECT_TRUE(WebRequestRulesRegistry::HostPermissionsChecker(
      extension_all_urls.get(), action_set.get(), &error));
  EXPECT_TRUE(error.empty()) << error;

  EXPECT_FALSE(WebRequestRulesRegistry::HostPermissionsChecker(
      extension_some_urls.get(), action_set.get(), &error));
  EXPECT_THAT(error, HasSubstr("permission for all"));
  EXPECT_THAT(error, HasSubstr(action_set->actions().back()->GetName()));

  EXPECT_FALSE(WebRequestRulesRegistry::HostPermissionsChecker(
      extension_no_url.get(), action_set.get(), &error));
  EXPECT_THAT(error, HasSubstr("permission for all"));
  EXPECT_THAT(error, HasSubstr(action_set->actions().back()->GetName()));
}

TEST_F(WebRequestRulesRegistryTest, CheckOriginAndPathRegEx) {
  const char kRule[] =
      "{                                                                 \n"
      "  \"id\": \"rule1\",                                              \n"
      "  \"conditions\": [                                               \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RequestMatcher\", \n"
      "      \"url\": {\"originAndPathMatches\": \"fo+.com\"}            \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"actions\": [                                                  \n"
      "    {                                                             \n"
      "      \"instanceType\": \"declarativeWebRequest.RedirectRequest\",\n"
      "      \"redirectUrl\": \"http://bar.com\"                         \n"
      "    }                                                             \n"
      "  ],                                                              \n"
      "  \"priority\": 200                                               \n"
      "}                                                                 ";

  base::Value::Dict value = base::test::ParseJsonDict(kRule);

  std::optional<api::events::Rule> rule = api::events::Rule::FromValue(value);
  ASSERT_TRUE(rule);
  std::vector<const api::events::Rule*> rules = {&rule.value()};

  scoped_refptr<WebRequestRulesRegistry> registry(
      new TestWebRequestRulesRegistry(&profile_));

  URLMatcher matcher;
  std::string error = registry->AddRulesImpl(kExtensionId, rules);
  EXPECT_EQ("", error);

  EventResponseDeltas deltas;

  // No match because match is in the query parameter.
  GURL url1("http://bar.com/index.html?foo.com");
  WebRequestInfo request1_info(CreateRequestParams(url1));
  WebRequestData request_data1(&request1_info, ON_BEFORE_REQUEST);
  deltas = registry->CreateDeltas(PermissionHelper::Get(&profile_),
                                  request_data1, false);
  EXPECT_EQ(0u, deltas.size());

  // This is a correct match.
  GURL url2("http://foo.com/index.html");
  WebRequestInfo request2_info(CreateRequestParams(url2));
  WebRequestData request_data2(&request2_info, ON_BEFORE_REQUEST);
  deltas = registry->CreateDeltas(PermissionHelper::Get(&profile_),
                                  request_data2, false);
  EXPECT_EQ(1u, deltas.size());
}

}  // namespace extensions
