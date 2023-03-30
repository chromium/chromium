// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <bitset>
#include <vector>
#include "base/base64.h"
#include "base/command_line.h"
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

// Token 3
// generate_token.py http://localhost Feature3 --expire-timestamp=2000000000
const char kToken3Encoded[] =
    "0MkZnXAAvIP08B1so7hGptn+TVzextwwPpov15HrC1G/"
    "ODiS+rcLf9VmTorMGvVeD1qtMLSp9fV3o+iRyqEZBw==";
const uint8_t kToken3Signature[kTokenSignatureSize] = {
    0xd0, 0xc9, 0x19, 0x9d, 0x70, 0x00, 0xbc, 0x83, 0xf4, 0xf0, 0x1d,
    0x6c, 0xa3, 0xb8, 0x46, 0xa6, 0xd9, 0xfe, 0x4d, 0x5c, 0xde, 0xc6,
    0xdc, 0x30, 0x3e, 0x9a, 0x2f, 0xd7, 0x91, 0xeb, 0x0b, 0x51, 0xbf,
    0x38, 0x38, 0x92, 0xfa, 0xb7, 0x0b, 0x7f, 0xd5, 0x66, 0x4e, 0x8a,
    0xcc, 0x1a, 0xf5, 0x5e, 0x0f, 0x5a, 0xad, 0x30, 0xb4, 0xa9, 0xf5,
    0xf5, 0x77, 0xa3, 0xe8, 0x91, 0xca, 0xa1, 0x19, 0x07};

// Token 4
// generate_token.py http://localhost Feature4 --expire-timestamp=2000000000
const char kToken4Encoded[] =
    "x6nmohz1AIru1NOk3AfAvAOqlO8YAbk34lwRRv97KcmYjnO0ty/"
    "rJoYK7+oy9XyjWlKPR+iIsEo6MJXBSes+Dw==";
const uint8_t kToken4Signature[kTokenSignatureSize] = {
    0xc7, 0xa9, 0xe6, 0xa2, 0x1c, 0xf5, 0x00, 0x8a, 0xee, 0xd4, 0xd3,
    0xa4, 0xdc, 0x07, 0xc0, 0xbc, 0x03, 0xaa, 0x94, 0xef, 0x18, 0x01,
    0xb9, 0x37, 0xe2, 0x5c, 0x11, 0x46, 0xff, 0x7b, 0x29, 0xc9, 0x98,
    0x8e, 0x73, 0xb4, 0xb7, 0x2f, 0xeb, 0x26, 0x86, 0x0a, 0xef, 0xea,
    0x32, 0xf5, 0x7c, 0xa3, 0x5a, 0x52, 0x8f, 0x47, 0xe8, 0x88, 0xb0,
    0x4a, 0x3a, 0x30, 0x95, 0xc1, 0x49, 0xeb, 0x3e, 0x0f};

// Token 5
// generate_token.py http://localhost Feature5 --expire-timestamp=2000000000
const char kToken5Encoded[] =
    "UAesWB38gFs4UxRMz+1v8i/"
    "AvwaBZsqawnq11ekBNFCvP+L6RGamKijtKHrmtAGhrrF0O3zrh+4J3CGdWaYQCQ==";
const uint8_t kToken5Signature[kTokenSignatureSize] = {
    0x50, 0x07, 0xac, 0x58, 0x1d, 0xfc, 0x80, 0x5b, 0x38, 0x53, 0x14,
    0x4c, 0xcf, 0xed, 0x6f, 0xf2, 0x2f, 0xc0, 0xbf, 0x06, 0x81, 0x66,
    0xca, 0x9a, 0xc2, 0x7a, 0xb5, 0xd5, 0xe9, 0x01, 0x34, 0x50, 0xaf,
    0x3f, 0xe2, 0xfa, 0x44, 0x66, 0xa6, 0x2a, 0x28, 0xed, 0x28, 0x7a,
    0xe6, 0xb4, 0x01, 0xa1, 0xae, 0xb1, 0x74, 0x3b, 0x7c, 0xeb, 0x87,
    0xee, 0x09, 0xdc, 0x21, 0x9d, 0x59, 0xa6, 0x10, 0x09};

// Token 6
// generate_token.py http://localhost Feature6 --expire-timestamp=2000000000
const char kToken6Encoded[] =
    "SrjYQZAV/sChC7CXzP6Hv6Tvun3FMcg87ary8hAtAXNBcZ0w2M+k1nNyvbbXPF0G5U/"
    "7DihaWyvaU6Oho+ecCg==";
const uint8_t kToken6Signature[kTokenSignatureSize] = {
    0x4a, 0xb8, 0xd8, 0x41, 0x90, 0x15, 0xfe, 0xc0, 0xa1, 0x0b, 0xb0,
    0x97, 0xcc, 0xfe, 0x87, 0xbf, 0xa4, 0xef, 0xba, 0x7d, 0xc5, 0x31,
    0xc8, 0x3c, 0xed, 0xaa, 0xf2, 0xf2, 0x10, 0x2d, 0x01, 0x73, 0x41,
    0x71, 0x9d, 0x30, 0xd8, 0xcf, 0xa4, 0xd6, 0x73, 0x72, 0xbd, 0xb6,
    0xd7, 0x3c, 0x5d, 0x06, 0xe5, 0x4f, 0xfb, 0x0e, 0x28, 0x5a, 0x5b,
    0x2b, 0xda, 0x53, 0xa3, 0xa1, 0xa3, 0xe7, 0x9c, 0x0a};

// Token 7
// generate_token.py http://localhost Feature7 --expire-timestamp=2000000000
const char kToken7Encoded[] =
    "MWABLgE6rUbcRXnu8O/"
    "zMQ0arYbDgTwAKxzVkSuhm28TI3PLQRO1nSFxcbJhsmlF77YhCKanuTe5bc0e+NI8DQ==";
const uint8_t kToken7Signature[kTokenSignatureSize] = {
    0x31, 0x60, 0x01, 0x2e, 0x01, 0x3a, 0xad, 0x46, 0xdc, 0x45, 0x79,
    0xee, 0xf0, 0xef, 0xf3, 0x31, 0x0d, 0x1a, 0xad, 0x86, 0xc3, 0x81,
    0x3c, 0x00, 0x2b, 0x1c, 0xd5, 0x91, 0x2b, 0xa1, 0x9b, 0x6f, 0x13,
    0x23, 0x73, 0xcb, 0x41, 0x13, 0xb5, 0x9d, 0x21, 0x71, 0x71, 0xb2,
    0x61, 0xb2, 0x69, 0x45, 0xef, 0xb6, 0x21, 0x08, 0xa6, 0xa7, 0xb9,
    0x37, 0xb9, 0x6d, 0xcd, 0x1e, 0xf8, 0xd2, 0x3c, 0x0d};

// Token 8
// generate_token.py http://localhost Feature8 --expire-timestamp=2000000000
const char kToken8Encoded[] =
    "M7tESBhBT4c3p3B8AKQ7EPyCxVq0tkro5FnaFcTMK7hxJxN94uabeRiQ6rew1CuDiv/"
    "be7psx1kRWQYJNgsOCQ==";
const uint8_t kToken8Signature[kTokenSignatureSize] = {
    0x33, 0xbb, 0x44, 0x48, 0x18, 0x41, 0x4f, 0x87, 0x37, 0xa7, 0x70,
    0x7c, 0x00, 0xa4, 0x3b, 0x10, 0xfc, 0x82, 0xc5, 0x5a, 0xb4, 0xb6,
    0x4a, 0xe8, 0xe4, 0x59, 0xda, 0x15, 0xc4, 0xcc, 0x2b, 0xb8, 0x71,
    0x27, 0x13, 0x7d, 0xe2, 0xe6, 0x9b, 0x79, 0x18, 0x90, 0xea, 0xb7,
    0xb0, 0xd4, 0x2b, 0x83, 0x8a, 0xff, 0xdb, 0x7b, 0xba, 0x6c, 0xc7,
    0x59, 0x11, 0x59, 0x06, 0x09, 0x36, 0x0b, 0x0e, 0x09};

// Token 9
// generate_token.py http://localhost Feature9 --expire-timestamp=2000000000
const char kToken9Encoded[] =
    "m7Pf/Nzxf26+5u3qx5ruUuR44L9iBoBl7O16Zgh/CEsFzYeTvjDOUQ7k8MpI/"
    "USRqytp8xfJD8d6g3LSlhyKAw==";
const uint8_t kToken9Signature[kTokenSignatureSize] = {
    0x9b, 0xb3, 0xdf, 0xfc, 0xdc, 0xf1, 0x7f, 0x6e, 0xbe, 0xe6, 0xed,
    0xea, 0xc7, 0x9a, 0xee, 0x52, 0xe4, 0x78, 0xe0, 0xbf, 0x62, 0x06,
    0x80, 0x65, 0xec, 0xed, 0x7a, 0x66, 0x08, 0x7f, 0x08, 0x4b, 0x05,
    0xcd, 0x87, 0x93, 0xbe, 0x30, 0xce, 0x51, 0x0e, 0xe4, 0xf0, 0xca,
    0x48, 0xfd, 0x44, 0x91, 0xab, 0x2b, 0x69, 0xf3, 0x17, 0xc9, 0x0f,
    0xc7, 0x7a, 0x83, 0x72, 0xd2, 0x96, 0x1c, 0x8a, 0x03};

// Token 10
// generate_token.py http://localhost Feature10 --expire-timestamp=2000000000
const char kToken10Encoded[] =
    "6RvhT78FdWLfwM7aRxorOBr8PG1z6BGzsEWH3tDZZrc3vQhM7VG3m19AX74noaAQ18F6LUG1Zd"
    "brhHELUpflDw==";
const uint8_t kToken10Signature[kTokenSignatureSize] = {
    0xe9, 0x1b, 0xe1, 0x4f, 0xbf, 0x05, 0x75, 0x62, 0xdf, 0xc0, 0xce,
    0xda, 0x47, 0x1a, 0x2b, 0x38, 0x1a, 0xfc, 0x3c, 0x6d, 0x73, 0xe8,
    0x11, 0xb3, 0xb0, 0x45, 0x87, 0xde, 0xd0, 0xd9, 0x66, 0xb7, 0x37,
    0xbd, 0x08, 0x4c, 0xed, 0x51, 0xb7, 0x9b, 0x5f, 0x40, 0x5f, 0xbe,
    0x27, 0xa1, 0xa0, 0x10, 0xd7, 0xc1, 0x7a, 0x2d, 0x41, 0xb5, 0x65,
    0xd6, 0xeb, 0x84, 0x71, 0x0b, 0x52, 0x97, 0xe5, 0x0f};

// Token 11
// generate_token.py http://localhost Feature11 --expire-timestamp=2000000000
const char kToken11Encoded[] =
    "vdHX5gXbkCVod3P93hkFWwBbTdXHvb+832BYMtTICKf8xyrbUATnfjBSxTWNNc+ptK+/"
    "RAr2jOWVHuH+YR+UBQ==";
const uint8_t kToken11Signature[kTokenSignatureSize] = {
    0xbd, 0xd1, 0xd7, 0xe6, 0x05, 0xdb, 0x90, 0x25, 0x68, 0x77, 0x73,
    0xfd, 0xde, 0x19, 0x05, 0x5b, 0x00, 0x5b, 0x4d, 0xd5, 0xc7, 0xbd,
    0xbf, 0xbc, 0xdf, 0x60, 0x58, 0x32, 0xd4, 0xc8, 0x08, 0xa7, 0xfc,
    0xc7, 0x2a, 0xdb, 0x50, 0x04, 0xe7, 0x7e, 0x30, 0x52, 0xc5, 0x35,
    0x8d, 0x35, 0xcf, 0xa9, 0xb4, 0xaf, 0xbf, 0x44, 0x0a, 0xf6, 0x8c,
    0xe5, 0x95, 0x1e, 0xe1, 0xfe, 0x61, 0x1f, 0x94, 0x05};

const DisabledTokensTestData kDisabledTokensTests[] = {
    // One token
    {
        {kToken1Encoded},
        {kToken1Signature},
    },
    // Two tokens
    {{kToken1Encoded, kToken2Encoded}, {kToken1Signature, kToken2Signature}},
    // Eleven tokens: Almost max
    // Closest to the max case of 1024 characters.
    // The size of the first eleven base64 encoded 64 character bitset is 978.
    // See SetupOriginTrialsCommandLine in
    // components/embedder_support/origin_trials/component_updater_utils.cc
    // When crbug.com/1216609 is addressed, this case should be adjusted to
    // accommodate a higher max.
    {{kToken1Encoded, kToken2Encoded, kToken3Encoded, kToken4Encoded,
      kToken5Encoded, kToken6Encoded, kToken7Encoded, kToken8Encoded,
      kToken9Encoded, kToken10Encoded, kToken11Encoded},
     {kToken1Signature, kToken2Signature, kToken3Signature, kToken4Signature,
      kToken5Signature, kToken6Signature, kToken7Signature, kToken8Signature,
      kToken9Signature, kToken10Signature, kToken11Signature}},
};

class ChromeOriginTrialsTest : public InProcessBrowserTest {
 public:
  ChromeOriginTrialsTest(const ChromeOriginTrialsTest&) = delete;
  ChromeOriginTrialsTest& operator=(const ChromeOriginTrialsTest&) = delete;

 protected:
  ChromeOriginTrialsTest() {}

  std::string GetCommandLineSwitch(const base::StringPiece& switch_name) {
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
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  EXPECT_FALSE(
      command_line->HasSwitch(embedder_support::kOriginTrialDisabledTokens));
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
  std::set<std::string> expected_signatures;
  std::transform(
      GetParam().expected_list.begin(), GetParam().expected_list.end(),
      std::inserter(expected_signatures, expected_signatures.begin()),
      [](const uint8_t bytes[]) -> std::string {
        return std::string(reinterpret_cast<const char*>(bytes),
                           kTokenSignatureSize);
      });
  blink::OriginTrialPolicy* policy =
      content::GetContentClientForTesting()->GetOriginTrialPolicy();
  ASSERT_TRUE(local_state()->HasPrefPath(
      embedder_support::prefs::kOriginTrialDisabledTokens));
  ASSERT_TRUE(policy);
  ASSERT_EQ(*(policy->GetDisabledTokensForTesting()), expected_signatures);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ChromeOriginTrialsDisabledTokensTest,
                         ::testing::ValuesIn(kDisabledTokensTests));

const uint16_t kAboveMaxTokensToProduce = 12;

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
      std::string encoded_token;
      std::string token = std::bitset<kTokenSignatureSize>(i).to_string();
      base::Base64Encode(token, &encoded_token);
      disabled_tokens.push_back(encoded_token);
    }
    return disabled_tokens;
  }
};  // namespace

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
