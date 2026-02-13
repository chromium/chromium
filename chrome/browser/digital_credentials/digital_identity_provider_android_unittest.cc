// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/digital_credentials/digital_identity_provider_android.h"

#include "base/values.h"
#include "content/public/browser/digital_identity_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;

TEST(DigitalIdentityProviderAndroidTest, ParseResultSuccess) {
  std::string result = "{\"key\": \"value\"}";
  auto expected_value = DigitalIdentityProviderAndroid::ParseResult(
      result, static_cast<int32_t>(RequestStatusForMetrics::kSuccess));
  ASSERT_TRUE(expected_value.has_value());
  EXPECT_EQ(*expected_value->GetDict().FindString("key"), "value");
}

TEST(DigitalIdentityProviderAndroidTest, ParseResultFailure) {
  std::string result = "";
  auto expected_value = DigitalIdentityProviderAndroid::ParseResult(
      result, static_cast<int32_t>(RequestStatusForMetrics::kErrorOther));
  ASSERT_FALSE(expected_value.has_value());
  EXPECT_EQ(expected_value.error(), RequestStatusForMetrics::kErrorOther);
}

TEST(DigitalIdentityProviderAndroidTest, ParseResultInvalidJson) {
  std::string result = "{key: value}";  // Invalid JSON (keys must be quoted)
  auto expected_value = DigitalIdentityProviderAndroid::ParseResult(
      result, static_cast<int32_t>(RequestStatusForMetrics::kSuccess));
  ASSERT_FALSE(expected_value.has_value());
  EXPECT_EQ(expected_value.error(), RequestStatusForMetrics::kErrorInvalidJson);
}

TEST(DigitalIdentityProviderAndroidTest, ParseResultStrictJson) {
  // Comments are not allowed in RFC JSON but were allowed in Chromium
  // extensions
  std::string result = "{\"key\": \"value\"} // comment";
  auto expected_value = DigitalIdentityProviderAndroid::ParseResult(
      result, static_cast<int32_t>(RequestStatusForMetrics::kSuccess));
  ASSERT_FALSE(expected_value.has_value());
  EXPECT_EQ(expected_value.error(), RequestStatusForMetrics::kErrorInvalidJson);
}
