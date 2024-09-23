// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/spaced/fake_spaced_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/login/auth/auth_events_recorder.h"
#endif

namespace system_logs {
namespace {

std::unique_ptr<SystemLogsResponse> GetChromeInternalLogs() {
  base::RunLoop run_loop;
  ChromeInternalLogSource source;
  std::unique_ptr<SystemLogsResponse> response;
  source.Fetch(
      base::BindLambdaForTesting([&](std::unique_ptr<SystemLogsResponse> r) {
        response = std::move(r);
        run_loop.Quit();
      }));
  run_loop.Run();
  return response;
}

class ChromeInternalLogSourceTest : public BrowserWithTestWindowTest {
 public:
  ChromeInternalLogSourceTest() = default;
  ChromeInternalLogSourceTest(const ChromeInternalLogSourceTest&) = delete;
  ChromeInternalLogSourceTest& operator=(const ChromeInternalLogSourceTest&) =
      delete;
  ~ChromeInternalLogSourceTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    auth_events_recorder_ = ash::AuthEventsRecorder::CreateForTesting();
#endif
    BrowserWithTestWindowTest::SetUp();
  }

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<ash::AuthEventsRecorder> auth_events_recorder_;
#endif
};

TEST_F(ChromeInternalLogSourceTest, VersionTagContainsActualVersion) {
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
TEST_F(ChromeInternalLogSourceTest, VersionTagContainsExtendedLabel) {
  chrome::ScopedChannelOverride channel_override(
      chrome::ScopedChannelOverride::Channel::kExtendedStable);

  ASSERT_TRUE(chrome::IsExtendedStableChannel());
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)

TEST_F(ChromeInternalLogSourceTest, SkiaGraphiteStatusPresentAndValid) {
  auto response = GetChromeInternalLogs();
  auto value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "unknown");

  content::GpuDataManager::GetInstance()->SetSkiaGraphiteEnabledForTesting(
      true);
  response = GetChromeInternalLogs();
  value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "enabled");

  content::GpuDataManager::GetInstance()->SetSkiaGraphiteEnabledForTesting(
      false);
  response = GetChromeInternalLogs();
  value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "disabled");
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
TEST_F(ChromeInternalLogSourceTest, CpuTypePresentAndValid) {
  auto response = GetChromeInternalLogs();
  auto value = response->at("cpu_arch");
#if BUILDFLAG(IS_MAC)
  switch (base::mac::GetCPUType()) {
    case base::mac::CPUType::kIntel:
      EXPECT_EQ(value, "x86-64");
      break;
    case base::mac::CPUType::kTranslatedIntel:
      EXPECT_EQ(value, "x86-64/translated");
      break;
    case base::mac::CPUType::kArm:
      EXPECT_EQ(value, "arm64");
      break;
  }
#else
#if defined(ARCH_CPU_ARM64)
  EXPECT_EQ(value, "arm64");
#else
  bool emulated = base::win::OSInfo::IsRunningEmulatedOnArm64();
#if defined(ARCH_CPU_X86)
  if (emulated) {
    EXPECT_EQ(value, "32-bit emulated");
  } else {
    EXPECT_EQ(value, "32-bit");
  }
#else   // defined(ARCH_CPU_X86)
  if (emulated) {
    EXPECT_EQ(value, "64-bit emulated");
  } else {
    EXPECT_EQ(value, "64-bit");
  }
#endif  // defined(ARCH_CPU_X86)
#endif  // defined(ARCH_CPU_ARM64)
#endif
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ChromeInternalLogSourceTest, FreeAndTotalDiskSpacePresent) {
  ash::SpacedClient::InitializeFake();
  ash::FakeSpacedClient::Get()->set_free_disk_space(1000);
  ash::FakeSpacedClient::Get()->set_total_disk_space(100000);

  std::unique_ptr<SystemLogsResponse> response = GetChromeInternalLogs();
  ASSERT_TRUE(response);
  auto free_disk_space = response->at("FREE_DISK_SPACE");
  auto total_disk_space = response->at("TOTAL_DISK_SPACE");

  EXPECT_EQ(free_disk_space, "1000");
  EXPECT_EQ(total_disk_space, "100000");
}

TEST_F(ChromeInternalLogSourceTest, KnowledgeFactorAuthFailuresPresent) {
  auth_events_recorder_->OnKnowledgeFactorAuthFailure();

  std::unique_ptr<SystemLogsResponse> response = GetChromeInternalLogs();
  auto knowledge_factor_auth_failure_count =
      response->at("FAILED_KNOWLEDGE_FACTOR_ATTEMPTS");

  EXPECT_EQ(knowledge_factor_auth_failure_count, "1");
}

TEST_F(ChromeInternalLogSourceTest, RecordedAuthEventsPresent) {
  auth_events_recorder_->OnAuthenticationSurfaceChange(
      ash::AuthEventsRecorder::AuthenticationSurface::kLogin);
  auth_events_recorder_->OnLockContentsViewUpdate();
  auth_events_recorder_->OnAuthSubmit();
  auth_events_recorder_->OnLoginSuccess(ash::SuccessReason::OFFLINE_ONLY,
                                        /*is_new_user=*/false,
                                        /*is_login_offline=*/true,
                                        /*is_ephemeral=*/false);
  auth_events_recorder_->OnExistingUserLoginScreenExit(
      ash::AuthEventsRecorder::AuthenticationOutcome::kSuccess, 1);

  std::unique_ptr<SystemLogsResponse> response = GetChromeInternalLogs();
  auto auth_events = response->at("RECORDED_AUTH_EVENTS");

  EXPECT_EQ(auth_events,
            "auth_surface_change_Login,update_lock_screen_view,auth_submit,"
            "login_offline,login_screen_exit_success,");
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace
}  // namespace system_logs
