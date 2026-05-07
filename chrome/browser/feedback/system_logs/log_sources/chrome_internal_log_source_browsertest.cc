// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include <memory>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/buildflags.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/updater/browser_updater_client_testutils.h"  // nogncheck
#include "chrome/browser/updater/updater.h"
#include "chrome/updater/constants.h"       // nogncheck
#include "chrome/updater/update_service.h"  // nogncheck
#include "chrome/updater/updater_scope.h"   // nogncheck
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/test/base/scoped_channel_override.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
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

class ChromeInternalLogSourceTest : public InProcessBrowserTest {
 public:
  ChromeInternalLogSourceTest() = default;
  ChromeInternalLogSourceTest(const ChromeInternalLogSourceTest&) = delete;
  ChromeInternalLogSourceTest& operator=(const ChromeInternalLogSourceTest&) =
      delete;
  ~ChromeInternalLogSourceTest() override = default;
};

IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       VersionTagContainsActualVersion) {
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING) && !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       VersionTagContainsExtendedLabel) {
  chrome::ScopedChannelOverride channel_override(
      chrome::ScopedChannelOverride::Channel::kExtendedStable);

  ASSERT_TRUE(chrome::IsExtendedStableChannel());
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}
#endif

IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       SkiaGraphiteStatusPresentAndValid) {
  content::GpuDataManager::GetInstance()->SetInitializedForTesting(false);
  auto response = GetChromeInternalLogs();
  auto value = response->at("skia_graphite_status");
  EXPECT_EQ(value, "unknown");

  content::GpuDataManager::GetInstance()->SetInitializedForTesting(true);
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
IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest, CpuTypePresentAndValid) {
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

#if BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       FreeAndTotalDiskSpacePresent) {
  ash::FakeSpacedClient::Get()->set_free_disk_space(1000);
  ash::FakeSpacedClient::Get()->set_total_disk_space(100000);

  std::unique_ptr<SystemLogsResponse> response = GetChromeInternalLogs();
  ASSERT_TRUE(response);
  auto free_disk_space = response->at("FREE_DISK_SPACE");
  auto total_disk_space = response->at("TOTAL_DISK_SPACE");

  EXPECT_EQ(free_disk_space, "1000");
  EXPECT_EQ(total_disk_space, "100000");
}

IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest,
                       KnowledgeFactorAuthFailuresPresent) {
  ash::AuthEventsRecorder::Get()->OnKnowledgeFactorAuthFailure();

  std::unique_ptr<SystemLogsResponse> response = GetChromeInternalLogs();
  auto knowledge_factor_auth_failure_count =
      response->at("FAILED_KNOWLEDGE_FACTOR_ATTEMPTS");

  EXPECT_EQ(knowledge_factor_auth_failure_count, "1");
}

IN_PROC_BROWSER_TEST_F(ChromeInternalLogSourceTest, RecordedAuthEventsPresent) {
  auto* auth_events_recorder = ash::AuthEventsRecorder::Get();
  auth_events_recorder->OnAuthenticationSurfaceChange(
      ash::AuthEventsRecorder::AuthenticationSurface::kLogin);
  auth_events_recorder->OnLockContentsViewUpdate();
  auth_events_recorder->OnAuthSubmit();
  auth_events_recorder->OnLoginSuccess(ash::SuccessReason::OFFLINE_ONLY,
                                       /*is_new_user=*/false,
                                       /*is_login_offline=*/true,
                                       /*is_ephemeral=*/false);
  auth_events_recorder->OnExistingUserLoginScreenExit(
      ash::AuthEventsRecorder::AuthenticationOutcome::kSuccess, 1);

  std::unique_ptr<SystemLogsResponse> response = GetChromeInternalLogs();
  auto auth_events = response->at("RECORDED_AUTH_EVENTS");

  EXPECT_EQ(auth_events,
            "auth_surface_change_Login,update_lock_screen_view,auth_submit,"
            "login_offline,login_screen_exit_success,");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace
}  // namespace system_logs
