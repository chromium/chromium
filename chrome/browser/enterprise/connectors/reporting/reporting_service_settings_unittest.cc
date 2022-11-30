// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#endif

namespace enterprise_connectors {

namespace {

constexpr char kNoProviderSettings[] = "{}";

constexpr char kNormalSettingsWithoutEvents[] =
    R"({ "service_provider": "google" })";

constexpr char kNormalSettingsWithEvents[] =
    R"({ "service_provider": "google",
         "enabled_event_names" : ["event 1", "event 2", "event 3"]
       })";

constexpr char kNormalSettingsWithOptInEvents[] =
    R"({ "service_provider": "google",
         "enabled_opt_in_events" : [
            { "name": "opt_in_event 1", "url_patterns" : []},
            { "name": "opt_in_event 2", "url_patterns" : ["*"]},
            {
              "name": "opt_in_event 3",
              "url_patterns" : ["example.com", "other.example.com"]
            }
          ]})";

}  // namespace

class ReportingServiceSettingsTest : public testing::Test {
 public:
  void SetUpTestCommandLine() {
    base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
    cmd->AppendSwitchASCII("reporting-connector-url",
                           "https://test.com/reports");
    policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();
  }

  absl::optional<ReportingSettings> GetReportingSettings(
      const char* settings_value) {
    auto settings = base::JSONReader::Read(settings_value,
                                           base::JSON_ALLOW_TRAILING_COMMAS);
    EXPECT_TRUE(settings.has_value());

    ReportingServiceSettings service_settings(settings.value(),
                                              *GetServiceProviderConfig());

    return service_settings.GetReportingSettings();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This is necessary so the URL flag code works on CrOS. If it's absent, a
  // CrOS DCHECK fails when trying to access the
  // BrowserPolicyConnectorAsh as it is not completely initialized.
  ash::ScopedCrosSettingsTestHelper cros_settings_;
#endif
};

TEST_F(ReportingServiceSettingsTest, TestNoSettings) {
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNoProviderSettings);
  ASSERT_FALSE(reporting_settings.has_value());
}

TEST_F(ReportingServiceSettingsTest, TestNormalSettingsWithoutEvents) {
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithoutEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  ASSERT_TRUE(reporting_settings->reporting_url.is_valid());
  ASSERT_EQ(GURL("https://chromereporting-pa.googleapis.com/v1/events"),
            reporting_settings.value().reporting_url);
}

TEST_F(ReportingServiceSettingsTest, TestNormalSettingsWithEvents) {
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  ASSERT_FALSE(reporting_settings->enabled_event_names.empty());
  std::set<std::string> expected_event_names{"event 1", "event 2", "event 3"};
  ASSERT_EQ(expected_event_names,
            reporting_settings.value().enabled_event_names);
}

TEST_F(ReportingServiceSettingsTest, TestNormalSettingsWithOptInEvents) {
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithOptInEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  std::map<std::string, std::vector<std::string>> actual_opt_in_events =
      reporting_settings.value().enabled_opt_in_events;
  ASSERT_EQ(2UL, actual_opt_in_events.size());

  // An event with no URL patterns isn't enabled.
  ASSERT_EQ(actual_opt_in_events.find("opt_in_event 1"),
            actual_opt_in_events.end());

  ASSERT_NE(actual_opt_in_events.find("opt_in_event 2"),
            actual_opt_in_events.end());
  ASSERT_EQ(1UL, actual_opt_in_events["opt_in_event 2"].size());

  ASSERT_NE(actual_opt_in_events.find("opt_in_event 3"),
            actual_opt_in_events.end());
  ASSERT_EQ(2UL, actual_opt_in_events["opt_in_event 3"].size());
}

TEST_F(ReportingServiceSettingsTest, FlagOverrideNoProviderSettings) {
  SetUpTestCommandLine();
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNoProviderSettings);
  ASSERT_FALSE(reporting_settings.has_value());
}

TEST_F(ReportingServiceSettingsTest, FlagOverrideNormalSettingsWithoutEvents) {
  SetUpTestCommandLine();
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithoutEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  ASSERT_TRUE(reporting_settings->reporting_url.is_valid());
  ASSERT_EQ(reporting_settings->reporting_url, "https://test.com/reports");
}

TEST_F(ReportingServiceSettingsTest, FlagOverrideNormalSettingsWithEvents) {
  SetUpTestCommandLine();
  absl::optional<ReportingSettings> reporting_settings =
      GetReportingSettings(kNormalSettingsWithEvents);
  ASSERT_TRUE(reporting_settings.has_value());

  ASSERT_TRUE(reporting_settings->reporting_url.is_valid());
  ASSERT_EQ(reporting_settings->reporting_url, "https://test.com/reports");
}

}  // namespace enterprise_connectors
