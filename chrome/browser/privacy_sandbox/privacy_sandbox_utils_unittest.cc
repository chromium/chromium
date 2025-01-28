// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/privacy_sandbox_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

using enum privacy_sandbox::PrivacyPolicyDomainType;
using enum privacy_sandbox::PrivacyPolicyColorScheme;

struct PrivacyPolicyURLTestData {
  // Inputs
  privacy_sandbox::PrivacyPolicyDomainType domain_type;
  privacy_sandbox::PrivacyPolicyColorScheme color_scheme;
  std::string locale;
  // Expectation
  std::string expected_url;
};

// This test class verifies the construction of the embedded privacy policy URL
// under different conditions, including variations in:
//  1. If the user is located in China (domain_type)
//  2. Browser Theme (color_scheme)
//  3. User's Language Preference (locale)
//
// It ensures the correct URL is generated for each combination of these
// parameters.
class PrivacyPolicyURLTests
    : public testing::Test,
      public testing::WithParamInterface<PrivacyPolicyURLTestData> {};

TEST_P(PrivacyPolicyURLTests, ValidatingURL) {
  std::string privacy_policy_url = privacy_sandbox::GetEmbeddedPrivacyPolicyURL(
      GetParam().domain_type, GetParam().color_scheme, GetParam().locale);
  ASSERT_EQ(privacy_policy_url, GetParam().expected_url);
}

INSTANTIATE_TEST_SUITE_P(
    PrivacyPolicyURLTests,
    PrivacyPolicyURLTests,
    testing::Values(
        PrivacyPolicyURLTestData{
            .domain_type = kChina,
            .color_scheme = kDarkMode,
            .locale = "fr",
            .expected_url = "https://policies.google.cn/privacy/"
                            "embedded?hl=fr&color_scheme=dark",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kNonChina,
            .color_scheme = kDarkMode,
            .locale = "fr",
            .expected_url = "https://policies.google.com/privacy/"
                            "embedded?hl=fr&color_scheme=dark",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kChina,
            .color_scheme = kLightMode,
            .locale = "fr",
            .expected_url = "https://policies.google.cn/privacy/embedded?hl=fr",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kNonChina,
            .color_scheme = kLightMode,
            .locale = "fr",
            .expected_url =
                "https://policies.google.com/privacy/embedded?hl=fr",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kChina,
            .color_scheme = kLightMode,
            .locale = "",
            .expected_url = "https://policies.google.cn/privacy/embedded",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kNonChina,
            .color_scheme = kLightMode,
            .locale = "",
            .expected_url = "https://policies.google.com/privacy/embedded",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kChina,
            .color_scheme = kDarkMode,
            .locale = "",
            .expected_url =
                "https://policies.google.cn/privacy/embedded?color_scheme=dark",
        },
        PrivacyPolicyURLTestData{
            .domain_type = kNonChina,
            .color_scheme = kDarkMode,
            .locale = "",
            .expected_url = "https://policies.google.com/privacy/"
                            "embedded?color_scheme=dark",
        },
        // Validating that this edge case in
        // google_util::AppendGoogleLocaleParam are covered by the returned URL.
        PrivacyPolicyURLTestData{
            .domain_type = kNonChina,
            .color_scheme = kLightMode,
            .locale = "nb",
            .expected_url =
                "https://policies.google.com/privacy/embedded?hl=no",
        }));
}  // namespace
