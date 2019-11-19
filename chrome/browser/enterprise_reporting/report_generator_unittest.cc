// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/report_generator.h"

#include <set>

#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

#if !defined(OS_CHROMEOS)
constexpr char kProfile[] = "Profile";

const char kPluginName[] = "plugin";
const char kPluginVersion[] = "1.0";
const char kPluginDescription[] = "This is a plugin.";
const char kPluginFileName[] = "file_name";

// We only upload serial number on Windows.
void VerifySerialNumber(const std::string& serial_number) {
#if defined(OS_WIN)
  EXPECT_NE(std::string(), serial_number);
#else
  EXPECT_EQ(std::string(), serial_number);
#endif
}

// Controls the way of Profile creation which affects report.
enum ProfileStatus {
  // Idle Profile does not generate full report.
  kIdle,
  // Active Profile generates full report.
  kActive,
  // Active Profile generate large full report.
  kActiveWithContent,
};

// Verify the name is in the set. Remove the name from the set afterwards.
void FindAndRemoveProfileName(std::set<std::string>* names,
                              const std::string& name) {
  auto it = names->find(name);
  EXPECT_NE(names->end(), it);
  names->erase(it);
}

void AddExtensionToProfile(TestingProfile* profile) {
  extensions::ExtensionRegistry* extension_registry =
      extensions::ExtensionRegistry::Get(profile);

  std::string extension_name =
      "a super super super super super super super super super super super "
      "super super super super super super long extension name";
  extension_registry->AddEnabled(extensions::ExtensionBuilder(extension_name)
                                     .SetID("abcdefghijklmnoabcdefghijklmnoab")
                                     .Build());
}

#endif
}  // namespace

#if !defined(OS_CHROMEOS)
class ReportGeneratorTest : public ::testing::Test {
 public:
  ReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());

    profile_manager_.CreateGuestProfile();
    profile_manager_.CreateSystemProfile();

    content::PluginService::GetInstance()->Init();
  }

  // Creates |number| of Profiles. Returns the set of their names. The profile
  // names begin with Profile|start_index|. Profile instances are created if
  // |is_active| is true. Otherwise, information is only put into
  // ProfileAttributesStorage.
  std::set<std::string> CreateProfiles(int number,
                                       ProfileStatus status,
                                       int start_index = 0,
                                       bool with_extension = false) {
    std::set<std::string> profile_names;
    for (int i = start_index; i < number; i++) {
      std::string profile_name =
          std::string(kProfile) + base::NumberToString(i);
      switch (status) {
        case kIdle:
          profile_manager_.profile_attributes_storage()->AddProfile(
              profile_manager()->profiles_dir().AppendASCII(profile_name),
              base::ASCIIToUTF16(profile_name), std::string(), base::string16(),
              false, 0, std::string(), EmptyAccountId());
          break;
        case kActive:
          profile_manager_.CreateTestingProfile(profile_name);
          break;
        case kActiveWithContent:
          TestingProfile* profile =
              profile_manager_.CreateTestingProfile(profile_name);
          AddExtensionToProfile(profile);
          break;
      }
      profile_names.insert(profile_name);
    }
    return profile_names;
  }

  void CreatePlugin() {
    content::WebPluginInfo info;
    info.name = base::ASCIIToUTF16(kPluginName);
    info.version = base::ASCIIToUTF16(kPluginVersion);
    info.desc = base::ASCIIToUTF16(kPluginDescription);
    info.path =
        base::FilePath().AppendASCII("path").AppendASCII(kPluginFileName);
    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RegisterInternalPlugin(info, true);
    plugin_service->RefreshPlugins();
  }

  std::vector<std::unique_ptr<em::ChromeDesktopReportRequest>>
  GenerateRequests() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    base::RunLoop run_loop;
    std::vector<std::unique_ptr<em::ChromeDesktopReportRequest>> rets;
    generator_.Generate(base::BindLambdaForTesting(
        [&run_loop, &rets](ReportGenerator::Requests requests) {
          while (!requests.empty()) {
            rets.push_back(std::move(requests.front()));
            requests.pop();
          }
          run_loop.Quit();
        }));
    run_loop.Run();
    VerifyMetrics(rets);
    return rets;
  }

  // Verify the profile report matches actual Profile setup.
  void VerifyProfileReport(const std::set<std::string>& active_profiles_names,
                           const std::set<std::string>& inactive_profiles_names,
                           const em::BrowserReport& actual_browser_report) {
    int expected_profile_number =
        active_profiles_names.size() + inactive_profiles_names.size();
    EXPECT_EQ(expected_profile_number,
              actual_browser_report.chrome_user_profile_infos_size());

    auto mutable_active_profiles_names(active_profiles_names);
    auto mutable_inactive_profiles_names(inactive_profiles_names);
    for (int i = 0; i < expected_profile_number; i++) {
      auto actual_profile_info =
          actual_browser_report.chrome_user_profile_infos(i);
      std::string actual_profile_name = actual_profile_info.name();

      // Verify that the profile id is set as profile path.
      EXPECT_EQ(profile_manager_.profiles_dir()
                    .AppendASCII(actual_profile_name)
                    .AsUTF8Unsafe(),
                actual_profile_info.id());

      EXPECT_TRUE(actual_profile_info.has_is_full_report());

      // Activate profiles have full report while the inactive ones don't.
      if (actual_profile_info.is_full_report())
        FindAndRemoveProfileName(&mutable_active_profiles_names,
                                 actual_profile_name);
      else
        FindAndRemoveProfileName(&mutable_inactive_profiles_names,
                                 actual_profile_name);
    }
  }

  void VerifyMetrics(
      std::vector<std::unique_ptr<em::ChromeDesktopReportRequest>>& rets) {
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingRequestCount", rets.size(), 1);
    histogram_tester_->ExpectUniqueSample(
        "Enterprise.CloudReportingBasicRequestSize",
        /*basic request size floor to KB*/ 0, 1);
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }
  ReportGenerator* generator() { return &generator_; }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

 private:
  ReportGenerator generator_;

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(ReportGeneratorTest);
};

TEST_F(ReportGeneratorTest, GenerateBasicReport) {
  auto profile_names = CreateProfiles(/*number*/ 2, kIdle);
  CreatePlugin();

  auto requests = GenerateRequests();
  EXPECT_EQ(1u, requests.size());

  auto* basic_request = requests[0].get();
  EXPECT_NE(std::string(), basic_request->computer_name());
  EXPECT_NE(std::string(), basic_request->os_user_name());
  VerifySerialNumber(basic_request->serial_number());

  EXPECT_TRUE(basic_request->has_os_report());
  auto& os_report = basic_request->os_report();
  EXPECT_NE(std::string(), os_report.name());
  EXPECT_NE(std::string(), os_report.arch());
  EXPECT_NE(std::string(), os_report.version());

  EXPECT_TRUE(basic_request->has_browser_report());
  auto& browser_report = basic_request->browser_report();
  EXPECT_NE(std::string(), browser_report.browser_version());
  EXPECT_NE(std::string(), browser_report.executable_path());
  EXPECT_TRUE(browser_report.has_channel());
  // There might be other plugins like PDF plugin, however, our fake plugin
  // should be the first one in the report.
  EXPECT_LE(1, browser_report.plugins_size());
  EXPECT_EQ(kPluginName, browser_report.plugins(0).name());
  EXPECT_EQ(kPluginVersion, browser_report.plugins(0).version());
  EXPECT_EQ(kPluginDescription, browser_report.plugins(0).description());
  EXPECT_EQ(kPluginFileName, browser_report.plugins(0).filename());

  VerifyProfileReport(/*active_profile_names*/ std::set<std::string>(),
                      profile_names, browser_report);
}

TEST_F(ReportGeneratorTest, GenerateActiveProfiles) {
  auto inactive_profiles_names = CreateProfiles(/*number*/ 2, kIdle);
  auto active_profiles_names =
      CreateProfiles(/*number*/ 2, kActive, /*start_index*/ 2);

  auto requests = GenerateRequests();
  EXPECT_EQ(1u, requests.size());

  VerifyProfileReport(active_profiles_names, inactive_profiles_names,
                      requests[0]->browser_report());

  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 1);
}

TEST_F(ReportGeneratorTest, BasicReportIsTooBig) {
  CreateProfiles(/*number*/ 2, kIdle);

  // Set a super small limitation.
  generator()->SetMaximumReportSizeForTesting(5);

  // Because the limitation is so small, no request can be created.
  auto requests = GenerateRequests();
  EXPECT_EQ(0u, requests.size());
  histogram_tester()->ExpectTotalCount("Enterprise.CloudReportingRequestSize",
                                       0);
}

TEST_F(ReportGeneratorTest, ReportSeparation) {
  auto profile_names =
      CreateProfiles(/*number*/ 2, kActiveWithContent, /*start_index*/ 0);

  // Set the limitation just below the size of the report so that it needs to be
  // separated into two requests later.
  auto requests = GenerateRequests();
  EXPECT_EQ(1u, requests.size());
  generator()->SetMaximumReportSizeForTesting(requests[0]->ByteSizeLong() - 30);

  std::set<std::string> first_request_profiles, second_request_profiles;
  first_request_profiles.insert(
      requests[0]->browser_report().chrome_user_profile_infos(0).name());
  second_request_profiles.insert(
      requests[0]->browser_report().chrome_user_profile_infos(1).name());

  requests = GenerateRequests();

  // The first profile is activated in the first request only while the second
  // profile is activated in the second request.
  EXPECT_EQ(2u, requests.size());
  VerifyProfileReport(first_request_profiles, second_request_profiles,
                      requests[0]->browser_report());
  VerifyProfileReport(second_request_profiles, first_request_profiles,
                      requests[1]->browser_report());
  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 2);
}

TEST_F(ReportGeneratorTest, ProfileReportIsTooBig) {
  std::set<std::string> first_profile_name =
      CreateProfiles(/*number*/ 1, kActiveWithContent, /*start_index*/ 0);

  // Set the limitation just below the size of the report.
  auto requests = GenerateRequests();
  EXPECT_EQ(1u, requests.size());
  generator()->SetMaximumReportSizeForTesting(requests[0]->ByteSizeLong() - 30);

  // Add a smaller Profile.
  auto second_profile_name =
      CreateProfiles(/*number*/ 1, kActive, /*start_index*/ 1);

  requests = GenerateRequests();

  EXPECT_EQ(1u, requests.size());
  // Only the second Profile is activated while the first one is too big to be
  // reported.
  VerifyProfileReport(second_profile_name, first_profile_name,
                      requests[0]->browser_report());
  histogram_tester()->ExpectBucketCount("Enterprise.CloudReportingRequestSize",
                                        /*report size floor to KB*/ 0, 2);
}

#endif

}  // namespace enterprise_reporting
