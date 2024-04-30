// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>
#include <string_view>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/embedder_support/origin_trials/pref_names.h"
#include "components/embedder_support/switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"

namespace {

struct DisabledItemsTestData {
  const std::vector<std::string> input_list;
  const std::string expected_switch;
};

static const char kNewPublicKey[] = "new public key";

const DisabledItemsTestData kDisabledFeaturesTests[] = {
    // One feature
    {{"A"}, "A"},
    // Two features
    {{"A", "B"}, "A|B"},
    // Three features
    {{"A", "B", "C"}, "A|B|C"},
    // Spaces in feature name
    {{"A", "B C"}, "A|B C"},
};

const uint8_t kTokenSignatureSize = 64;

struct DisabledTokensTestData {
  const std::vector<std::string> input_list;
  const std::vector<const uint8_t*> expected_list;
};

// Token 1
// generate_token.py http://localhost Feature1 --expire-timestamp=2000000000
const char kToken1Encoded[] =
    "99GJNOMWP/ydg7K674KYqH+OZUVVIdI5rO8Eu7DZIRszrTBrVDSbsRxziNVXcen2dy/"
    "n3l5xSRTw/g24SAYoDA==";
const uint8_t kToken1Signature[kTokenSignatureSize] = {
    0xf7, 0xd1, 0x89, 0x34, 0xe3, 0x16, 0x3f, 0xfc, 0x9d, 0x83, 0xb2,
    0xba, 0xef, 0x82, 0x98, 0xa8, 0x7f, 0x8e, 0x65, 0x45, 0x55, 0x21,
    0xd2, 0x39, 0xac, 0xef, 0x04, 0xbb, 0xb0, 0xd9, 0x21, 0x1b, 0x33,
    0xad, 0x30, 0x6b, 0x54, 0x34, 0x9b, 0xb1, 0x1c, 0x73, 0x88, 0xd5,
    0x57, 0x71, 0xe9, 0xf6, 0x77, 0x2f, 0xe7, 0xde, 0x5e, 0x71, 0x49,
    0x14, 0xf0, 0xfe, 0x0d, 0xb8, 0x48, 0x06, 0x28, 0x0c};

// Token 2
// generate_token.py http://localhost Feature2 --expire-timestamp=2000000000
const char kToken2Encoded[] =
    "NgVAW3t6zrfpq2WOpBKz9wpcQST0Lwckd/LFEG6iWZvwJlkR9l/yHbBkiAYHWPjREVBAz/"
    "9BgynZe5kIgC5TCw==";
const uint8_t kToken2Signature[kTokenSignatureSize] = {
    0x36, 0x05, 0x40, 0x5b, 0x7b, 0x7a, 0xce, 0xb7, 0xe9, 0xab, 0x65,
    0x8e, 0xa4, 0x12, 0xb3, 0xf7, 0x0a, 0x5c, 0x41, 0x24, 0xf4, 0x2f,
    0x07, 0x24, 0x77, 0xf2, 0xc5, 0x10, 0x6e, 0xa2, 0x59, 0x9b, 0xf0,
    0x26, 0x59, 0x11, 0xf6, 0x5f, 0xf2, 0x1d, 0xb0, 0x64, 0x88, 0x06,
    0x07, 0x58, 0xf8, 0xd1, 0x11, 0x50, 0x40, 0xcf, 0xff, 0x41, 0x83,
    0x29, 0xd9, 0x7b, 0x99, 0x08, 0x80, 0x2e, 0x53, 0x0b};

const DisabledTokensTestData kDisabledTokensTests[] = {
    // One token
    {
        {kToken1Encoded},
        {kToken1Signature},
    },
    // Two tokens
    {{kToken1Encoded, kToken2Encoded}, {kToken1Signature, kToken2Signature}},
};

class ChromeOriginTrialsTest : public InProcessBrowserTest {
 public:
  ChromeOriginTrialsTest(const ChromeOriginTrialsTest&) = delete;
  ChromeOriginTrialsTest& operator=(const ChromeOriginTrialsTest&) = delete;

 protected:
  ChromeOriginTrialsTest() {}

  std::string GetCommandLineSwitch(std::string_view switch_name) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    EXPECT_TRUE(command_line->HasSwitch(switch_name));
    return command_line->GetSwitchValueASCII(switch_name);
  }

  void AddDisabledFeaturesToPrefs(const std::vector<std::string>& features) {
    base::Value::List disabled_feature_list;
    for (const std::string& feature : features) {
      disabled_feature_list.Append(feature);
    }
    local_state()->SetList(
        embedder_support::prefs::kOriginTrialDisabledFeatures,
        std::move(disabled_feature_list));
  }

  void AddDisabledTokensToPrefs(const std::vector<std::string>& tokens) {
    base::Value::List disabled_token_list;
    for (const std::string& token : tokens) {
      disabled_token_list.Append(token);
    }
    local_state()->SetList(embedder_support::prefs::kOriginTrialDisabledTokens,
                           std::move(disabled_token_list));
  }

  PrefService* local_state() { return g_browser_process->local_state(); }
};

// Tests to verify that the command line is not set, when no prefs exist for
// the various updates.

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, NoPublicKeySet) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(
      command_line->HasSwitch(embedder_support::kOriginTrialPublicKey));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, NoDisabledFeatures) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(
      command_line->HasSwitch(embedder_support::kOriginTrialDisabledFeatures));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, NoDisabledTokens) {
  blink::OriginTrialPolicy* policy =
      content::GetContentClientForTesting()->GetOriginTrialPolicy();
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetDisabledTokensForTesting()->size(), 0UL);
}

// Tests to verify that the public key is correctly read from prefs and
// added to the command line
IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, PRE_PublicKeySetOnCommandLine) {
  local_state()->Set(embedder_support::prefs::kOriginTrialPublicKey,
                     base::Value(kNewPublicKey));
  ASSERT_EQ(kNewPublicKey, local_state()->GetString(
                               embedder_support::prefs::kOriginTrialPublicKey));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsTest, PublicKeySetOnCommandLine) {
  ASSERT_EQ(kNewPublicKey, local_state()->GetString(
                               embedder_support::prefs::kOriginTrialPublicKey));
  std::string actual =
      GetCommandLineSwitch(embedder_support::kOriginTrialPublicKey);
  EXPECT_EQ(kNewPublicKey, actual);
}

// Tests to verify that disabled features are correctly read from prefs and
// added to the command line
class ChromeOriginTrialsDisabledFeaturesTest
    : public ChromeOriginTrialsTest,
      public ::testing::WithParamInterface<DisabledItemsTestData> {};

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledFeaturesTest,
                       PRE_DisabledFeaturesSetOnCommandLine) {
  AddDisabledFeaturesToPrefs(GetParam().input_list);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));
}

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledFeaturesTest,
                       DisabledFeaturesSetOnCommandLine) {
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledFeatures));
  std::string actual =
      GetCommandLineSwitch(embedder_support::kOriginTrialDisabledFeatures);
  EXPECT_EQ(GetParam().expected_switch, actual);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeOriginTrialsDisabledFeaturesTest,
                         ::testing::ValuesIn(kDisabledFeaturesTests));

// Tests to verify that disabled tokens are correctly read from prefs and
// added to the OriginTrialPolicy.
class ChromeOriginTrialsDisabledTokensTest
    : public ChromeOriginTrialsTest,
      public ::testing::WithParamInterface<DisabledTokensTestData> {};

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledTokensTest,
                       PRE_DisabledTokensInPolicy) {
  AddDisabledTokensToPrefs(GetParam().input_list);
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
}

IN_PROC_BROWSER_TEST_P(ChromeOriginTrialsDisabledTokensTest,
                       DisabledTokensInPolicy) {
  // Convert the uint8_t[] from generate_token.py into strings.
  std::vector<std::string> expected_signatures;
  base::ranges::transform(
      GetParam().expected_list, std::back_inserter(expected_signatures),
      [](const uint8_t bytes[]) {
        return std::string(reinterpret_cast<const char*>(bytes),
                           kTokenSignatureSize);
      });
  blink::OriginTrialPolicy* policy =
      content::GetContentClientForTesting()->GetOriginTrialPolicy();
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
  ASSERT_TRUE(policy);
  EXPECT_THAT(*policy->GetDisabledTokensForTesting(),
              testing::UnorderedElementsAreArray(expected_signatures));
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeOriginTrialsDisabledTokensTest,
                         ::testing::ValuesIn(kDisabledTokensTests));

// Should match kMaxDisabledTokens in
// components/embedder_support/origin_trials/origin_trials_settings_storage.cc
const uint16_t kMaxTokensToProduce = 1024;
const uint16_t kAboveMaxTokensToProduce = 1025;

// Tests to verify the behavior of the policy once the number of disabled tokens
// provided exceeds the limit.

class ChromeOriginTrialsDisabledTokensLimitTest
    : public ChromeOriginTrialsTest {
 public:
  ChromeOriginTrialsDisabledTokensLimitTest() = default;

  std::vector<std::string> CreateDisabledTokens(uint16_t token_count) {
    // According to OriginTrialPolicyImpl, signatures must be 64 characters in
    // length. Create a bitset of 64 length then append the encoded version to
    // list of disabled tokens.
    std::vector<std::string> disabled_tokens;
    for (uint16_t i = 0; i < token_count; i++) {
      std::string token = std::bitset<kTokenSignatureSize>(i).to_string();
      std::string encoded_token = base::Base64Encode(token);
      disabled_tokens.push_back(encoded_token);
    }
    return disabled_tokens;
  }
};  // namespace

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsDisabledTokensLimitTest,
                       PRE_DisabledTokensMaxTokensAppliedToPolicy) {
  AddDisabledTokensToPrefs(CreateDisabledTokens(kMaxTokensToProduce));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsDisabledTokensLimitTest,
                       DisabledTokensMaxTokensAppliedToPolicy) {
  blink::OriginTrialPolicy* policy =
      content::GetContentClientForTesting()->GetOriginTrialPolicy();
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetDisabledTokensForTesting()->size(), kMaxTokensToProduce);
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsDisabledTokensLimitTest,
                       PRE_DisabledTokensOverSizeLimitAreIgnored) {
  AddDisabledTokensToPrefs(CreateDisabledTokens(kAboveMaxTokensToProduce));
}

IN_PROC_BROWSER_TEST_F(ChromeOriginTrialsDisabledTokensLimitTest,
                       DisabledTokensOverSizeLimitAreIgnored) {
  blink::OriginTrialPolicy* policy =
      content::GetContentClientForTesting()->GetOriginTrialPolicy();
  ASSERT_TRUE(policy);
  ASSERT_EQ(policy->GetDisabledTokensForTesting()->size(), 0UL);
}

}  // namespace
