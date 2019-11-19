// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"

#include <map>
#include <set>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

// A SHA256 hash for "mydomain.com".
const char kDomainHash[] =
    "0a79eaf6adb7b1e60d3fa548aa63105f525a00448efbb59ee965b9351a90ac31";

bool IsPromptEnabled() {
  return base::FeatureList::IsEnabled(kSettingsResetPrompt);
}

// Test class that initializes a ScopedFeatureList so that all tests
// start off with all features disabled.
class SettingsResetPromptConfigTest : public ::testing::Test {
 protected:
  typedef base::FieldTrialParams Parameters;

  // Sets the settings reset prompt feature parameters, which has the
  // side-effect of also enabling the feature.
  void SetFeatureParams(const Parameters& params) {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        kSettingsResetPrompt, params);
  }

  Parameters GetDefaultFeatureParams() {
    return {
        {"domain_hashes", base::StringPrintf("{\"%s\": \"1\"}", kDomainHash)},
        {"delay_before_prompt_seconds", "42"},
        {"prompt_wave", "20170101"},
        {"time_between_prompts_seconds", "3600"}};
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SettingsResetPromptConfigTest, Create) {
  ASSERT_FALSE(IsPromptEnabled());

  // |Create()| should return nullptr when feature is not enabled.
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // |Create()| should return false when feature is enabled, but parameters are
  // |missing.
  scoped_feature_list_.InitAndEnableFeature(kSettingsResetPrompt);
  ASSERT_TRUE(IsPromptEnabled());
  EXPECT_FALSE(SettingsResetPromptConfig::Create());
  SetFeatureParams(Parameters());
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // |Create()| should return a config when parameters are all present.
  // Individual parameters are tested separately.
  SetFeatureParams(GetDefaultFeatureParams());
  EXPECT_TRUE(SettingsResetPromptConfig::Create());
}

TEST_F(SettingsResetPromptConfigTest, DomainHashesParam) {
  Parameters params = GetDefaultFeatureParams();

  // First, test bad values for the "domain_hashes" parameter.

  // Parameter is missing.
  ASSERT_EQ(params.erase("domain_hashes"), 1U);
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Parameter is an empty string.
  params["domain_hashes"] = "";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Invalid JSON.
  params["domain_hashes"] = "bad json";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Parameter is not a JSON dictionary.
  params["domain_hashes"] = "[3]";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Bad dictionary key.
  params["domain_hashes"] = "\"bad key\": \"1\"";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Dictionary key is too short.
  params["domain_hashes"] = "\"1234abc\": \"1\"";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Dictionary key has correct length, but is not a hex string.
  std::string non_hex_key(64, 'x');
  params["domain_hashes"] =
      base::StringPrintf("{\"%s\": \"1\"}", non_hex_key.c_str());
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Correct key but non-integer value.
  params["domain_hashes"] =
      base::StringPrintf("{\"%s\": \"not integer\"}", kDomainHash);
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Correct key but integer value that is too big.
  std::string too_big_int(99, '1');
  params["domain_hashes"] =
      base::StringPrintf("{\"%s\": \"%s\"}", kDomainHash, too_big_int.c_str());
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Correct key but negative integer value.
  params["domain_hashes"] = base::StringPrintf("{\"%s\": \"-2\"}", kDomainHash);
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Should return non-nullptr with a correct set of parameters.
  SetFeatureParams(GetDefaultFeatureParams());
  EXPECT_TRUE(SettingsResetPromptConfig::Create());
}

TEST_F(SettingsResetPromptConfigTest, UrlToResetDomainId) {
  SetFeatureParams(GetDefaultFeatureParams());
  auto config = SettingsResetPromptConfig::Create();
  ASSERT_TRUE(config);

  // Should return negative value for URL with no match in the config.
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://www.hello.com")), 0);

  // Should return 1, which is "mydomain.com"'s ID.
  EXPECT_EQ(config->UrlToResetDomainId(GURL("http://www.sub.mydomain.com")), 1);
  EXPECT_EQ(config->UrlToResetDomainId(GURL("http://www.mydomain.com")), 1);
  EXPECT_EQ(config->UrlToResetDomainId(GURL("http://mydomain.com")), 1);

  // These URLs should not match "mydomain.com".
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://mydomain")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://mydomain.org")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://prefixmydomain.com")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://mydomain.com.com")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://www.mydomain.com.com")), 0);

  // Should return negative value for invalid URLs.
  EXPECT_LT(config->UrlToResetDomainId(GURL("htp://mydomain.com")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://mydomain com")), 0);
}

TEST_F(SettingsResetPromptConfigTest, UrlToResetDomainIdTLDs) {
  // Ensure that we do not match top level or registry domains even in the
  // presence of faulty config data that would match those.

  // Hash for "com".
  const char kTLDHash1[] =
      "71b4f3a3748cd6843c01e293e701fce769f52381821e21daf2ff4fe9ea57a6f3";
  // Hash for "co.uk".
  const char kTLDHash2[] =
      "ad4fad2f5e5fb480ff7f9f648c6b20bbd5e44362d86821e29d30e65e626299b0";
  // Hash for "uk".
  const char kTLDHash3[] =
      "83116acf18e4dc4414762f584ff43d9979ff2c2b0e9e48fbc97b21e23d7004ec";
  // Hash for "com.br".
  const char kTLDHash4[] =
      "1d9c4ffc5429a9b4529abf6fbe9f20b52b7401c8f0fed46c7ed67b1e3153932c";
  // Hash for private registry domain "appspot.com".
  const char kTLDHash5[] =
      "bffd48c8162466106a84f42945bfbbcfe501c9f0931219e02ce46e275f05ba51";

  Parameters params = GetDefaultFeatureParams();
  params["domain_hashes"] = base::StringPrintf(
      "{\"%s\": \"1\", \"%s\": \"2\", \"%s\": \"3\", \"%s\": \"4\", "
      "\"%s\": \"5\"}",
      kTLDHash1, kTLDHash2, kTLDHash3, kTLDHash4, kTLDHash5);
  SetFeatureParams(params);
  auto config = SettingsResetPromptConfig::Create();
  ASSERT_TRUE(config);

  EXPECT_LT(config->UrlToResetDomainId(GURL("http://something.com")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://something.co.uk")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://something.uk")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://something.com.br")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://something.appspot.com")),
            0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://com")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://co.uk")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://uk")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://com.br")), 0);
  EXPECT_LT(config->UrlToResetDomainId(GURL("http://appspot.com")), 0);
}

TEST_F(SettingsResetPromptConfigTest, DelayBeforePromptSecondsParam) {
  constexpr const char kDelayParam[] = "delay_before_prompt_seconds";

  Parameters params = GetDefaultFeatureParams();

  // Missing parameter.
  ASSERT_EQ(params.erase(kDelayParam), 1U);
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Empty parameter.
  params[kDelayParam] = "";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Bad parameter value.
  params[kDelayParam] = "not-a-number";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Negative parameter value.
  params[kDelayParam] = "-3";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Correct parameter value.
  params[kDelayParam] = "12";
  SetFeatureParams(params);
  {
    auto config = SettingsResetPromptConfig::Create();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->delay_before_prompt(), base::TimeDelta::FromSeconds(12));
  }

  // Correct edge case parameter value.
  params["delay_before_prompt_seconds"] = "0";
  SetFeatureParams(params);
  {
    auto config = SettingsResetPromptConfig::Create();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->delay_before_prompt(), base::TimeDelta::FromSeconds(0));
  }
}

TEST_F(SettingsResetPromptConfigTest, PromptWaveParam) {
  constexpr const char kPromptWaveParam[] = "prompt_wave";

  Parameters params = GetDefaultFeatureParams();

  // Missing parameter.
  ASSERT_EQ(params.erase(kPromptWaveParam), 1U);
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Empty parameter.
  params[kPromptWaveParam] = "";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Bad parameter value.
  params[kPromptWaveParam] = "not-a-number";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Negative parameter value.
  params[kPromptWaveParam] = "-3";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Correct parameter value.
  params[kPromptWaveParam] = "20170202";
  SetFeatureParams(params);
  {
    auto config = SettingsResetPromptConfig::Create();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->prompt_wave(), 20170202);
  }
}

TEST_F(SettingsResetPromptConfigTest, TimeBetweenPromptsParam) {
  constexpr const char kParamName[] = "time_between_prompts_seconds";

  Parameters params = GetDefaultFeatureParams();

  // Missing parameter.
  ASSERT_EQ(params.erase(kParamName), 1U);
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Empty parameter.
  params[kParamName] = "";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Bad parameter value.
  params[kParamName] = "not-a-number";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Negative parameter value.
  params[kParamName] = "-3";
  SetFeatureParams(params);
  EXPECT_FALSE(SettingsResetPromptConfig::Create());

  // Correct parameter value.
  params[kParamName] = "3600";
  SetFeatureParams(params);
  {
    auto config = SettingsResetPromptConfig::Create();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->time_between_prompts(),
              base::TimeDelta::FromSeconds(3600));
  }
}

}  // namespace safe_browsing
