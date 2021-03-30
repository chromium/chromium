// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_service_settings.h"

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
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

struct TestParam {
  TestParam(const char* settings_value, ReportingSettings* expected_settings)
      : settings_value(settings_value), expected_settings(expected_settings) {}

  const char* settings_value;
  ReportingSettings* expected_settings;
};

constexpr char kNoProviderSettings[] = "{}";

constexpr char kNormalSettingsWithoutEvents[] =
    R"({ "service_provider": "google" })";

constexpr char kNormalSettingsWithEvents[] =
    R"({ "service_provider": "google",
         "enabled_event_names" : ["event 1", "event 2", "event 3"]
       })";

ReportingSettings* NoSettings() {
  return nullptr;
}

ReportingSettings* NormalSettingsWithoutEvents() {
  static base::NoDestructor<ReportingSettings> settings;
  return settings.get();
}

ReportingSettings* NormalSettingsWithEvents() {
  static base::NoDestructor<ReportingSettings> settings;
  return settings.get();
}

}  // namespace

class ReportingServiceSettingsTest : public testing::TestWithParam<TestParam> {
 public:
  const char* settings_value() const { return GetParam().settings_value; }
  ReportingSettings* expected_settings() const {
    // Set the settings fields dynamically to avoid static initialization issue.
    if (GetParam().expected_settings == NormalSettingsWithoutEvents() &&
        !GetParam().expected_settings->reporting_url.is_valid()) {
      GetParam().expected_settings->reporting_url =
          GURL("https://chromereporting-pa.googleapis.com/v1/events");
    } else if (GetParam().expected_settings == NormalSettingsWithEvents() &&
               GetParam().expected_settings->enabled_event_names.empty()) {
      GetParam().expected_settings->enabled_event_names.insert("event 1");
      GetParam().expected_settings->enabled_event_names.insert("event 2");
      GetParam().expected_settings->enabled_event_names.insert("event 3");
    }
    return GetParam().expected_settings;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // This is necessary so the URL flag code works on CrOS. If it's absent, a
  // CrOS DCHECK fails when trying to access the
  // BrowserPolicyConnectorChromeOS as it is not completely initialized.
  ash::ScopedCrosSettingsTestHelper cros_settings_;
#endif
};

TEST_P(ReportingServiceSettingsTest, Test) {
  auto settings = base::JSONReader::Read(settings_value(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  ServiceProviderConfig config(kServiceProviderConfig);
  ReportingServiceSettings service_settings(settings.value(), config);

  auto reporting_settings = service_settings.GetReportingSettings();
  ASSERT_EQ((expected_settings() != nullptr), reporting_settings.has_value());
  if (expected_settings() == NormalSettingsWithoutEvents()) {
    ASSERT_TRUE(reporting_settings->reporting_url.is_valid());
    ASSERT_EQ(expected_settings()->reporting_url,
              reporting_settings.value().reporting_url);
  } else if (expected_settings() == NormalSettingsWithEvents()) {
    ASSERT_FALSE(reporting_settings->enabled_event_names.empty());
    ASSERT_EQ(expected_settings()->enabled_event_names,
              reporting_settings.value().enabled_event_names);
  } else {
    ASSERT_EQ(expected_settings(), NoSettings());
  }
}

TEST_P(ReportingServiceSettingsTest, FlagOverride) {
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  cmd->AppendSwitchASCII("reporting-connector-url", "https://test.com/reports");
  policy::ChromeBrowserPolicyConnector::EnableCommandLineSupportForTesting();

  auto settings = base::JSONReader::Read(settings_value(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  ServiceProviderConfig config(kServiceProviderConfig);
  ReportingServiceSettings service_settings(settings.value(), config);

  auto reporting_settings = service_settings.GetReportingSettings();
  ASSERT_EQ((expected_settings() != nullptr), reporting_settings.has_value());
  if (expected_settings()) {
    ASSERT_TRUE(reporting_settings->reporting_url.is_valid());
    ASSERT_EQ(reporting_settings->reporting_url, "https://test.com/reports");
  }
}

INSTANTIATE_TEST_CASE_P(
    ,
    ReportingServiceSettingsTest,
    testing::Values(
        TestParam(kNoProviderSettings, NoSettings()),
        TestParam(kNormalSettingsWithoutEvents, NormalSettingsWithoutEvents()),
        TestParam(kNormalSettingsWithEvents, NormalSettingsWithEvents())));

}  // namespace enterprise_connectors
