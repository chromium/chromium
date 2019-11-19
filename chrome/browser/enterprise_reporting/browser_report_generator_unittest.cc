// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/browser_report_generator.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/enterprise_reporting/profile_report_generator.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const char kProfileId[] = "profile_id";
const char kProfileName[] = "profile_name";

const char kPluginName[] = "plugin_name";
const char kPluginVersion[] = "plugin_version";
const char kPluginDescription[] = "plugin_description";
const char kPluginFolderPath[] = "plugin_folder_path";
const char kPluginFileName[] = "plugin_file_name";

}  // namespace

class BrowserReportGeneratorTest : public ::testing::Test {
 public:
  BrowserReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~BrowserReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_manager_.CreateGuestProfile();
    profile_manager_.CreateSystemProfile();
    content::PluginService::GetInstance()->Init();
  }

  void InitializeProfile() {
    profile_manager_.profile_attributes_storage()->AddProfile(
        profile_manager()->profiles_dir().AppendASCII(kProfileId),
        base::ASCIIToUTF16(kProfileName), std::string(), base::string16(),
        false, 0, std::string(), EmptyAccountId());
  }

  void InitializePlugin() {
    content::WebPluginInfo info;
    info.name = base::ASCIIToUTF16(kPluginName);
    info.version = base::ASCIIToUTF16(kPluginVersion);
    info.desc = base::ASCIIToUTF16(kPluginDescription);
    info.path = base::FilePath()
                    .AppendASCII(kPluginFolderPath)
                    .AppendASCII(kPluginFileName);

    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RegisterInternalPlugin(info, true);
    plugin_service->RefreshPlugins();
  }

  void GenerateAndVerify() {
    base::RunLoop run_loop;
    generator_.Generate(base::BindLambdaForTesting(
        [&run_loop](std::unique_ptr<em::BrowserReport> report) {
          EXPECT_TRUE(report.get());

          EXPECT_NE(std::string(), report->browser_version());
          EXPECT_NE(std::string(), report->executable_path());
          EXPECT_TRUE(report->has_channel());

          EXPECT_EQ(1, report->chrome_user_profile_infos_size());
          em::ChromeUserProfileInfo profile =
              report->chrome_user_profile_infos(0);
          EXPECT_NE(std::string(), profile.id());
          EXPECT_EQ(kProfileName, profile.name());
          EXPECT_FALSE(profile.is_full_report());

          EXPECT_LE(1, report->plugins_size());
          em::Plugin plugin = report->plugins(0);
          EXPECT_EQ(kPluginName, plugin.name());
          EXPECT_EQ(kPluginVersion, plugin.version());
          EXPECT_EQ(kPluginFileName, plugin.filename());
          EXPECT_EQ(kPluginDescription, plugin.description());
          run_loop.Quit();
        }));
    run_loop.Run();
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  BrowserReportGenerator generator_;

  DISALLOW_COPY_AND_ASSIGN(BrowserReportGeneratorTest);
};

TEST_F(BrowserReportGeneratorTest, GenerateBasicReport) {
  InitializeProfile();
  InitializePlugin();
  GenerateAndVerify();
}

}  // namespace enterprise_reporting
