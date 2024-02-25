// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"

#include "base/values.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace reporting {
namespace {

constexpr char kTestPolicyName[] = "TestPolicy";

TEST(MetricReportingPrefsTest,
     DisallowWebsiteMetricsReportingForUnsupportedUrls) {
  static constexpr char kUnsupportedUrl[] = "chrome://policy";
  base::Value::List policy_setting_value;
  policy_setting_value.Append(kUnsupportedUrl);

  test::FakeReportingSettings reporting_settings;
  reporting_settings.SetList(kTestPolicyName, std::move(policy_setting_value));

  EXPECT_FALSE(IsWebsiteUrlAllowlisted(GURL(kUnsupportedUrl),
                                       &reporting_settings, kTestPolicyName));
}

TEST(MetricReportingPrefsTest, DisallowWebsiteMetricsReportingIfPolicyUnset) {
  static constexpr char kUrl[] = "https://a.example.org";
  test::FakeReportingSettings reporting_settings;
  EXPECT_FALSE(IsWebsiteUrlAllowlisted(GURL(kUrl), &reporting_settings,
                                       kTestPolicyName));
}

TEST(MetricReportingPrefsTest, AllowWebsiteMetricsReportingWithWildcardValue) {
  static constexpr char kUrl[] = "https://a.example.org";
  base::Value::List policy_setting_value;
  policy_setting_value.Append(ContentSettingsPattern::Wildcard().ToString());

  test::FakeReportingSettings reporting_settings;
  reporting_settings.SetList(kTestPolicyName, std::move(policy_setting_value));

  EXPECT_TRUE(IsWebsiteUrlAllowlisted(GURL(kUrl), &reporting_settings,
                                      kTestPolicyName));
}

TEST(MetricReportingPrefsTest, AllowWebsiteMetricsReportingWithValidMatch) {
  static constexpr char kValidUrlPattern[] = "https://[*.]example.org";
  static constexpr char kUrl[] = "https://a.example.org/some/path";
  base::Value::List policy_setting_value;
  policy_setting_value.Append(kValidUrlPattern);

  test::FakeReportingSettings reporting_settings;
  reporting_settings.SetList(kTestPolicyName, std::move(policy_setting_value));

  EXPECT_TRUE(IsWebsiteUrlAllowlisted(GURL(kUrl), &reporting_settings,
                                      kTestPolicyName));
}

TEST(MetricReportingPrefsTest, DisallowWebsiteMetricsReportingIfNoMatch) {
  static constexpr char kUrl[] = "https://a.example.org";
  static constexpr char kOtherUrl[] = "https://www.google.com";
  base::Value::List policy_setting_value;
  policy_setting_value.Append(kUrl);

  test::FakeReportingSettings reporting_settings;
  reporting_settings.SetList(kTestPolicyName, std::move(policy_setting_value));

  EXPECT_FALSE(IsWebsiteUrlAllowlisted(GURL(kOtherUrl), &reporting_settings,
                                       kTestPolicyName));
}

TEST(MetricReportingPrefsTest, AllowWebsiteMetricsReporting_MultiplePatterns) {
  static constexpr char kUrl[] = "https://a.example.org/some/path";
  static constexpr char kMatchingUrlPattern[] = "https://[*.]example.org";
  static constexpr char kInvalidUrlPattern[] = "https://:";
  base::Value::List policy_setting_value;
  policy_setting_value.Append(kInvalidUrlPattern);
  policy_setting_value.Append(kMatchingUrlPattern);

  test::FakeReportingSettings reporting_settings;
  reporting_settings.SetList(kTestPolicyName, std::move(policy_setting_value));

  EXPECT_TRUE(IsWebsiteUrlAllowlisted(GURL(kUrl), &reporting_settings,
                                      kTestPolicyName));
}

TEST(MetricReportingPrefsTest,
     DisallowWebsiteMetricsReporting_MultiplePatterns) {
  static constexpr char kUrl[] = "https://a.example.org";
  static constexpr char kUrlPattern[] = "https://www.google.com";
  static constexpr char kInvalidUrlPattern[] = "https://:";
  base::Value::List policy_setting_value;
  policy_setting_value.Append(kInvalidUrlPattern);
  policy_setting_value.Append(kUrlPattern);

  test::FakeReportingSettings reporting_settings;
  reporting_settings.SetList(kTestPolicyName, std::move(policy_setting_value));

  EXPECT_FALSE(IsWebsiteUrlAllowlisted(GURL(kUrl), &reporting_settings,
                                       kTestPolicyName));
}

}  // namespace
}  // namespace reporting
