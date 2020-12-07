// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting_service_settings.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

struct TestParam {
  TestParam(const char* settings_value, ReportingSettings* expected_settings)
      : settings_value(settings_value), expected_settings(expected_settings) {}

  const char* settings_value;
  ReportingSettings* expected_settings;
};

constexpr char kNormalSettings[] = R"({ "service_provider": "google" })";

constexpr char kNoProviderSettings[] = "{}";

ReportingSettings* NormalSettings() {
  static base::NoDestructor<ReportingSettings> settings;
  return settings.get();
}

ReportingSettings* NoSettings() {
  return nullptr;
}

}  // namespace

class ReportingServiceSettingsTest : public testing::TestWithParam<TestParam> {
 public:
  const char* settings_value() const { return GetParam().settings_value; }
  ReportingSettings* expected_settings() const {
    // Set the GURL field dynamically to avoid static initialization issues.
    if (GetParam().expected_settings == NormalSettings() &&
        !GetParam().expected_settings->reporting_url.is_valid()) {
      GetParam().expected_settings->reporting_url =
          GURL("https://chromereporting-pa.googleapis.com/v1/events");
    }
    return GetParam().expected_settings;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(ReportingServiceSettingsTest, Test) {
  auto settings = base::JSONReader::Read(settings_value(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  ServiceProviderConfig config(kServiceProviderConfig);
  ReportingServiceSettings service_settings(settings.value(), config);

  auto reporting_settings = service_settings.GetReportingSettings();
  ASSERT_EQ((expected_settings() != nullptr), reporting_settings.has_value());
  if (reporting_settings.has_value()) {
    ASSERT_EQ(expected_settings(), NormalSettings());
    ASSERT_EQ(expected_settings()->reporting_url,
              reporting_settings.value().reporting_url);
  }
}

INSTANTIATE_TEST_CASE_P(
    ,
    ReportingServiceSettingsTest,
    testing::Values(TestParam(kNormalSettings, NormalSettings()),
                    TestParam(kNoProviderSettings, NoSettings())));

}  // namespace enterprise_connectors
