// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/chrome_desktop_report_request_helper.h"

#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/api/enterprise_reporting_private/prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace extensions {

class ChromeDesktopReportRequestGeneratorTest : public ::testing::Test {
 protected:
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
};

TEST_F(ChromeDesktopReportRequestGeneratorTest, OSInfo) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::string expected_os_info;

  expected_os_info = base::StringPrintf(
      R"({"arch":"%s","os":"%s","os_version":"%s"})",
      policy::GetOSArchitecture().c_str(), policy::GetOSPlatform().c_str(),
      policy::GetOSVersion().c_str());
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_os_info, request->os_info());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, MachineName) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::string expected_machine_name;

  expected_machine_name = base::StringPrintf(R"({"computername":"%s"})",
                                             policy::GetMachineName().c_str());
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_machine_name, request->machine_name());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, OSUsername) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::string expected_os_username, os_username_escaped;

  // The username needs to be escaped as the name can contain slashes.
  base::EscapeJSONString(policy::GetOSUsername(), false, &os_username_escaped);
  expected_os_username =
      base::StringPrintf(R"({"username":"%s"})", os_username_escaped.c_str());

  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_os_username, request->os_user());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, ProfileName) {
  // Set the profile name to a known value to compare against.
  const std::string test_name("TEST");
  profile_.GetPrefs()->SetString(prefs::kProfileName, test_name);

  // An empty report suffices for this test. The information of interest is
  // sourced from the profile
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);

  // Make sure the user name was set in the proto.
  EXPECT_EQ(test_name,
            request->browser_report().chrome_user_profile_reports(0).name());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, ExtensionList) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::unique_ptr<base::DictionaryValue> report;
  std::string expected_extension_list;

  // Extension list will be a empty list by default.
  expected_extension_list = "[]";
  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport": {"chromeUserProfileReport":[{}]}})"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_extension_list, request->browser_report()
                                         .chrome_user_profile_reports(0)
                                         .extension_data());

  // Extension list will be copied from the |report|.
  expected_extension_list = R"([{"id":"1\\\""}])";
  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
           {"chromeUserProfileReport":
             [
               {"extensionData": [{"id":"1\\\""}]}
             ]
           }
         })"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_extension_list, request->browser_report()
                                         .chrome_user_profile_reports(0)
                                         .extension_data());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, PluginList) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::unique_ptr<base::DictionaryValue> report;
  std::string expected_plugin_list;

  expected_plugin_list = R"([{"id":"1\\\""}])";
  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
           {"chromeUserProfileReport":
             [
               {"plugins": [{"id":"1\\\""}]}
             ]
           }
         })"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_plugin_list,
            request->browser_report().chrome_user_profile_reports(0).plugins());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, SignedInUser) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  std::unique_ptr<base::DictionaryValue> report;
  std::string expected_signed_in_user;

  expected_signed_in_user = R"({"email":"a\\@example.com"})";
  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
           {"chromeUserProfileReport":
             [
               {"chromeSignInUser": {"email":"a\\@example.com"}}
             ]
           }
         })"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(expected_signed_in_user, request->browser_report()
                                         .chrome_user_profile_reports(0)
                                         .chrome_signed_in_user());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, ProfileID) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;

  // |chrome_user_profile_report| will be created with Profile ID by default.
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(profile_.GetPath().AsUTF8Unsafe(),
            request->browser_report().chrome_user_profile_reports(0).id());

  // Profile ID will be merged into the first item of
  // |chrome_user_profile_reports|
  std::unique_ptr<base::DictionaryValue> report = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(R"({"browserReport":
                                  {"chromeUserProfileReport":[
                                    {"extensionData": [{"id":"1"}]}
                                  ]}
                                })"));
  ASSERT_TRUE(report);
  request = GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(profile_.GetPath().AsUTF8Unsafe(),
            request->browser_report().chrome_user_profile_reports(0).id());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, InvalidInput) {
  // |request| will not be generated if the type of input is invalid.
  std::unique_ptr<base::DictionaryValue> report;
  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
         {"chromeUserProfileReport":[{"extensionData":{}}]}})"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));

  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
         {"chromeUserProfileReport":[{"chromeSignInUser":""}]}})"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, SafeBrowsing) {
  profile_.GetPrefs()->SetBoolean(enterprise_reporting::kReportSafeBrowsingData,
                                  true);

  std::unique_ptr<base::DictionaryValue> report;
  report = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(R"({"browserReport":
                                  {"chromeUserProfileReport":[{
                                    "safeBrowsingWarnings":"invalid"
                                    }]
                                  }
                                })"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));

  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
           {"chromeUserProfileReport":[{
             "safeBrowsingWarningsClickThrough": "invalid"}]
           }
         })"));
  ASSERT_TRUE(report);
  EXPECT_FALSE(GenerateChromeDesktopReportRequest(*report, &profile_));

  report = base::DictionaryValue::From(base::JSONReader::ReadDeprecated(
      R"({"browserReport":
           {"chromeUserProfileReport":[{
              "safeBrowsingWarnings":3,
              "safeBrowsingWarningsClickThrough":1
             }]
           }
         })"));
  ASSERT_TRUE(report);
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_EQ(3u, request->browser_report()
                    .chrome_user_profile_reports(0)
                    .safe_browsing_warnings());
  EXPECT_EQ(1u, request->browser_report()
                    .chrome_user_profile_reports(0)
                    .safe_browsing_warnings_click_through());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, DontReportPolicyData) {
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetBoolean(enterprise_reporting::kReportPolicyData, false);

  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);

  ASSERT_TRUE(request);
  const em::ChromeUserProfileReport& profile =
      request->browser_report().chrome_user_profile_reports(0);
  EXPECT_FALSE(profile.has_policy_data());
  EXPECT_FALSE(profile.has_policy_fetched_timestamp());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest,
       DontReportExtensionsOrPluginsData) {
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetBoolean(enterprise_reporting::kReportExtensionsAndPluginsData,
                    false);

  std::unique_ptr<base::DictionaryValue> report = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(R"({"browserReport":
                                  {"chromeUserProfileReport":[
                                    {"extensionData": [{"id":"1"}],
                                     "plugins": [{"id":"2"}]
                                    }]}})"));
  ASSERT_TRUE(report);
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_FALSE(request->browser_report()
                   .chrome_user_profile_reports(0)
                   .has_extension_data());
  EXPECT_FALSE(
      request->browser_report().chrome_user_profile_reports(0).has_plugins());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, DontReportSafeBrowsingData) {
  PrefService* prefs = profile_.GetPrefs();
  prefs->SetBoolean(enterprise_reporting::kReportSafeBrowsingData, false);

  std::unique_ptr<base::DictionaryValue> report = base::DictionaryValue::From(
      base::JSONReader::ReadDeprecated(R"({"browserReport":
                                  {"chromeUserProfileReport":[
                                    {"safeBrowsingWarnings" : 1,
                                     "safeBrowsingWarningsClickThrough": 2
                                    }]}})"));
  ASSERT_TRUE(report);
  std::unique_ptr<em::ChromeDesktopReportRequest> request =
      GenerateChromeDesktopReportRequest(*report, &profile_);
  ASSERT_TRUE(request);
  EXPECT_FALSE(request->browser_report()
                   .chrome_user_profile_reports(0)
                   .has_safe_browsing_warnings());
  EXPECT_FALSE(request->browser_report()
                   .chrome_user_profile_reports(0)
                   .has_safe_browsing_warnings_click_through());
}

TEST_F(ChromeDesktopReportRequestGeneratorTest, SerialNumberNotEmpty) {
  std::unique_ptr<em::ChromeDesktopReportRequest> request;
  request =
      GenerateChromeDesktopReportRequest(base::DictionaryValue(), &profile_);
  ASSERT_TRUE(request);
#if defined(OS_WIN)
  EXPECT_NE(request->serial_number(), std::string());
#else
  EXPECT_TRUE(request->serial_number().empty());
#endif
}

}  // namespace extensions
