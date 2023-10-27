// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/common/metrics/app_package_name_logging_rule.h"

#include "base/time/time.h"
#include "base/version.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace android_webview {

namespace {

constexpr char kTestAllowlistVersion[] = "123.456.789.10";
}  // namespace

class AppPackageNameLoggingRuleTest : public testing::Test {
 public:
  AppPackageNameLoggingRuleTest() = default;

  AppPackageNameLoggingRuleTest& operator=(
      const AppPackageNameLoggingRuleTest&) = delete;
  AppPackageNameLoggingRuleTest(AppPackageNameLoggingRuleTest&&) = delete;
  AppPackageNameLoggingRuleTest& operator=(AppPackageNameLoggingRuleTest&&) =
      delete;
};

TEST_F(AppPackageNameLoggingRuleTest, TestFromDictionary) {
  base::Version version(kTestAllowlistVersion);
  base::Time one_day_from_now = base::Time::Now() + base::Days(1);
  {
    AppPackageNameLoggingRule expected_record(version, one_day_from_now);
    absl::optional<AppPackageNameLoggingRule> record =
        AppPackageNameLoggingRule::FromDictionary(
            expected_record.ToDictionary());
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(expected_record.IsSameAs(record.value()));
  }

  {
    AppPackageNameLoggingRule expected_record(version, base::Time::Min());
    absl::optional<AppPackageNameLoggingRule> record =
        AppPackageNameLoggingRule::FromDictionary(
            expected_record.ToDictionary());
    ASSERT_TRUE(record.has_value());
    EXPECT_TRUE(expected_record.IsSameAs(record.value()));
  }

  {
    absl::optional<AppPackageNameLoggingRule> record =
        AppPackageNameLoggingRule::FromDictionary(base::Value::Dict());
    EXPECT_FALSE(record.has_value());
  }
}

}  // namespace android_webview
