// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/account_id/account_id.h"
#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/report_util.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_task_environment.h"
#include "device_management_backend.pb.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/upgrade_detector/build_state.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/common/chrome_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/scoped_channel_override.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"
#else
#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace em = enterprise_management;

namespace enterprise_reporting {
namespace {

const char kProfileId[] = "profile_id";
const char kProfileName[] = "profile_name";
const char16_t kProfileName16[] = u"profile_name";

#if BUILDFLAG(ENABLE_PLUGINS)
const char16_t kPluginName16[] = u"plugin_name";
const char16_t kPluginVersion16[] = u"plugin_version";
const char16_t kPluginDescription16[] = u"plugin_description";
const char kPluginFolderPath[] = "plugin_folder_path";
const char kPluginFileName[] = "plugin_file_name";
#endif  // BUILDFLAG(ENABLE_PLUGINS)

#if BUILDFLAG(ENABLE_PLUGINS) && !BUILDFLAG(IS_CHROMEOS_ASH)
const char kPluginName[] = "plugin_name";
const char kPluginVersion[] = "plugin_version";
const char kPluginDescription[] = "plugin_description";
#endif  // BUILDFLAG(ENABLE_PLUGINS) && !BUILDFLAG(IS_CHROMEOS_ASH)

void VerifyBrowserVersionAndChannel(em::BrowserReport* report,
                                    bool with_version_info) {
  if (with_version_info) {
    EXPECT_NE(std::string(), report->browser_version());
    EXPECT_TRUE(report->has_channel());
  } else {
    EXPECT_FALSE(report->has_browser_version());
    EXPECT_FALSE(report->has_channel());
    EXPECT_FALSE(report->has_installed_browser_version());
  }
}

void VerifyBuildState(em::BrowserReport* report, bool with_version_info) {
#if !BUILDFLAG(IS_ANDROID)
  if (!with_version_info)
    return;
  const auto* build_state = g_browser_process->GetBuildState();
  if (build_state->update_type() == BuildState::UpdateType::kNone ||
      !build_state->installed_version()) {
    EXPECT_FALSE(report->has_installed_browser_version());
  } else {
    EXPECT_EQ(report->installed_browser_version(),
              build_state->installed_version()->GetString());
  }
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

void VerifyExtendedStableChannel(em::BrowserReport* report) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (chrome::IsExtendedStableChannel()) {
    EXPECT_TRUE(report->has_is_extended_stable_channel());
    EXPECT_TRUE(report->is_extended_stable_channel());
    EXPECT_EQ(report->channel(), em::Channel::CHANNEL_STABLE);
  } else {
    EXPECT_FALSE(report->has_is_extended_stable_channel());
    // On Android, local Chrome branded builds report "CHANNEL_UNKNOWN".
#if !BUILDFLAG(IS_ANDROID)
    EXPECT_NE(report->channel(), em::Channel::CHANNEL_UNKNOWN);
#endif
  }
#else
  EXPECT_FALSE(report->has_is_extended_stable_channel());
  EXPECT_EQ(report->channel(), em::Channel::CHANNEL_UNKNOWN);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
}

void VerifyProfile(em::BrowserReport* report) {
  EXPECT_EQ(1, report->chrome_user_profile_infos_size());
  em::ChromeUserProfileInfo profile = report->chrome_user_profile_infos(0);
  EXPECT_NE(std::string(), profile.id());
  EXPECT_EQ(kProfileName, profile.name());
  EXPECT_FALSE(profile.is_detail_available());
}

void VerifyPlugins(em::BrowserReport* report) {
#if BUILDFLAG(ENABLE_PLUGINS) && !BUILDFLAG(IS_ANDROID) && \
    !BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_LE(1, report->plugins_size());
  em::Plugin plugin = report->plugins(0);
  EXPECT_EQ(kPluginName, plugin.name());
  EXPECT_EQ(kPluginVersion, plugin.version());
  EXPECT_EQ(kPluginFileName, plugin.filename());
  EXPECT_EQ(kPluginDescription, plugin.description());
#else
  EXPECT_EQ(0, report->plugins_size());
#endif  // BUILDFLAG(ENABLE_PLUGINS) && !BUILDFLAG(IS_ANDROID) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

#if BUILDFLAG(IS_ANDROID)
typedef ReportingDelegateFactoryAndroid PlatformReportingDelegateFactory;
#else
typedef ReportingDelegateFactoryDesktop PlatformReportingDelegateFactory;
#endif  // BUILDFLAG(IS_ANDROID)

class BrowserReportGeneratorTest : public ::testing::Test {
 public:
  BrowserReportGeneratorTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()),
        generator_(&delegate_factory_) {}

  BrowserReportGeneratorTest(const BrowserReportGeneratorTest&) = delete;
  BrowserReportGeneratorTest& operator=(const BrowserReportGeneratorTest&) =
      delete;

  ~BrowserReportGeneratorTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
#if BUILDFLAG(ENABLE_PLUGINS)
    content::PluginService::GetInstance()->Init();
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  }

  void InitializeProfile() {
    ProfileAttributesInitParams params;
    params.profile_path =
        profile_manager()->profiles_dir().AppendASCII(kProfileId);
    params.profile_name = kProfileName16;
    profile_manager_.profile_attributes_storage()->AddProfile(
        std::move(params));
  }

  void InitializeIrregularProfiles() {
    profile_manager_.CreateGuestProfile();
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
    profile_manager_.CreateSystemProfile();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
    profile_manager_.CreateTestingProfile(chrome::kInitialProfile);
    profile_manager_.CreateTestingProfile(chrome::kLockScreenAppProfile);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

  void InitializePlugin() {
#if BUILDFLAG(ENABLE_PLUGINS)
    content::WebPluginInfo info;
    info.name = kPluginName16;
    info.version = kPluginVersion16;
    info.desc = kPluginDescription16;
    info.path = base::FilePath()
                    .AppendASCII(kPluginFolderPath)
                    .AppendASCII(kPluginFileName);

    content::PluginService* plugin_service =
        content::PluginService::GetInstance();
    plugin_service->RegisterInternalPlugin(info, true);
    plugin_service->RefreshPlugins();
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  }

#if !BUILDFLAG(IS_ANDROID)
  void InitializeUpdate() {
    auto* build_state = g_browser_process->GetBuildState();
    build_state->SetUpdate(BuildState::UpdateType::kNormalUpdate,
                           base::Version("1.2.3.4"), absl::nullopt);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  void GenerateAndVerify() {
    base::RunLoop run_loop;
    generator_.Generate(
        ReportType::kFull,
        base::BindLambdaForTesting(
            [&run_loop](std::unique_ptr<em::BrowserReport> report) {
              ASSERT_TRUE(report.get());
              EXPECT_EQ(
                  base::PathService::CheckedGet(base::DIR_EXE).AsUTF8Unsafe(),
                  report->executable_path());
#if BUILDFLAG(IS_CHROMEOS_ASH)
              bool with_version_info = false;
#else
              bool with_version_info = true;
#endif  // if BUILDFLAG(IS_CHROMEOS_ASH)
              VerifyBrowserVersionAndChannel(report.get(), with_version_info);
              VerifyBuildState(report.get(), with_version_info);
              VerifyExtendedStableChannel(report.get());
              VerifyProfile(report.get());
              VerifyPlugins(report.get());

              run_loop.Quit();
            }));
    run_loop.Run();
  }

  void GenerateProfileReportAndVerify() {
    base::RunLoop run_loop;
    generator_.Generate(
        ReportType::kProfileReport,
        base::BindLambdaForTesting(
            [&run_loop](std::unique_ptr<em::BrowserReport> report) {
              ASSERT_TRUE(report.get());
              EXPECT_EQ(
                  ObfuscateFilePath(base::PathService::CheckedGet(base::DIR_EXE)
                                        .AsUTF8Unsafe()),
                  report->executable_path());

              VerifyBrowserVersionAndChannel(report.get(),
                                             /*with_version_info=*/true);
              VerifyBuildState(report.get(), /*with_version_info=*/true);
              VerifyExtendedStableChannel(report.get());
              EXPECT_LE(0, report->plugins_size());

              // There should be no profile information.
              EXPECT_EQ(0, report->chrome_user_profile_infos_size());
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  TestingProfileManager* profile_manager() { return &profile_manager_; }

 private:
  PlatformReportingDelegateFactory delegate_factory_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  BrowserReportGenerator generator_;
};

TEST_F(BrowserReportGeneratorTest, GenerateBasicReport) {
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateAndVerify();
}

TEST_F(BrowserReportGeneratorTest, GenerateBasicReportForProfileReporting) {
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateProfileReportAndVerify();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(BrowserReportGeneratorTest, GenerateBasicReportWithUpdate) {
  InitializeUpdate();
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateAndVerify();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) && \
    BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(BrowserReportGeneratorTest, ExtendedStableChannel) {
  chrome::ScopedChannelOverride channel_override(
      chrome::ScopedChannelOverride::Channel::kExtendedStable);

  ASSERT_TRUE(chrome::IsExtendedStableChannel());
  InitializeProfile();
  InitializeIrregularProfiles();
  InitializePlugin();
  GenerateAndVerify();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace enterprise_reporting
