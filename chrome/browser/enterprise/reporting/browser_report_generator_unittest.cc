// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

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
#include "content/public/test/browser_task_environment.h"
#include "device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/upgrade_detector/build_state.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/test/base/scoped_channel_override.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)

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

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

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
    profile_manager_.CreateTestingProfile(ash::kSigninBrowserContextBaseName);
    profile_manager_.CreateTestingProfile(
        ash::kLockScreenAppBrowserContextBaseName);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  }

#if !BUILDFLAG(IS_ANDROID)
  void InitializeUpdate() {
    auto* build_state = g_browser_process->GetBuildState();
    build_state->SetUpdate(BuildState::UpdateType::kNormalUpdate,
                           base::Version("1.2.3.4"), std::nullopt);
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
  GenerateAndVerify();
}

TEST_F(BrowserReportGeneratorTest, GenerateBasicReportForProfileReporting) {
  InitializeProfile();
  InitializeIrregularProfiles();
  GenerateProfileReportAndVerify();
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(BrowserReportGeneratorTest, GenerateBasicReportWithUpdate) {
  InitializeUpdate();
  InitializeProfile();
  InitializeIrregularProfiles();
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
  GenerateAndVerify();
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH) &&
        // BUILDFLAG(GOOGLE_CHROME_BRANDING)

}  // namespace enterprise_reporting
