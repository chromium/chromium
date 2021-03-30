// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/chrome_internal_log_source.h"

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
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

using ChromeInternalLogSourceTest = BrowserWithTestWindowTest;

TEST_F(ChromeInternalLogSourceTest, VersionTagContainsActualVersion) {
  auto response = GetChromeInternalLogs();
  EXPECT_PRED_FORMAT2(
      testing::IsSubstring,
      chrome::GetVersionString(chrome::WithExtendedStable(true)),
      response->at("CHROME VERSION"));
}

#if defined(OS_MAC)
TEST_F(ChromeInternalLogSourceTest, CpuTypePresentAndValid) {
  auto response = GetChromeInternalLogs();
  auto value = response->at("cpu_arch");
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
}
#endif

}  // namespace
}  // namespace system_logs
