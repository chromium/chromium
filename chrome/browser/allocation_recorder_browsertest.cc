// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugging_buildflags.h"
#include "base/path_service.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/allocation_recorder/testing/crash_verification.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
#include "base/cpu.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/base_switches.h"
#include "base/command_line.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

class AllocationRecorderBrowserTest : public PlatformBrowserTest {
 public:
  AllocationRecorderBrowserTest();
  ~AllocationRecorderBrowserTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void CrashRendererProcess();
};

AllocationRecorderBrowserTest::AllocationRecorderBrowserTest() = default;
AllocationRecorderBrowserTest::~AllocationRecorderBrowserTest() = default;

void AllocationRecorderBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  command_line->AppendSwitch(switches::kEnableCrashpad);
#endif
}

void AllocationRecorderBrowserTest::CrashRendererProcess() {
  const GURL crash_url("chrome://crash");

  auto* const web_contents = chrome_test_utils::GetActiveWebContents(this);

  const content::ScopedAllowRendererCrashes allow_renderer_crashes(
      web_contents);

  ASSERT_FALSE(content::NavigateToURL(web_contents, crash_url))
      << "Loading crash url didn't crash the browser. url='" << crash_url
      << '\'';
}

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)

IN_PROC_BROWSER_TEST_F(AllocationRecorderBrowserTest,
                       VerifyCrashreportIncludesRecorder) {
  base::FilePath crashpad_database_path;

  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crashpad_database_path));

  const bool expect_report_with_content =
      base::CPU::GetInstanceNoAllocation().has_mte();

  allocation_recorder::testing::
      VerifyCrashCreatesCrashpadReportWithAllocationRecorderStream(
          crashpad_database_path,
          base::BindOnce(&AllocationRecorderBrowserTest::CrashRendererProcess,
                         base::Unretained(this)),
          base::BindOnce(&allocation_recorder::testing::VerifyPayload,
                         expect_report_with_content));
}

#else

IN_PROC_BROWSER_TEST_F(AllocationRecorderBrowserTest,
                       VerifyCrashreportIncludesNoRecorder) {
  base::FilePath crashpad_database_path;

  ASSERT_TRUE(
      base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crashpad_database_path));

  allocation_recorder::testing::
      VerifyCrashCreatesCrashpadReportWithoutAllocationRecorderStream(
          crashpad_database_path,
          base::BindOnce(&AllocationRecorderBrowserTest::CrashRendererProcess,
                         base::Unretained(this)));
}
#endif
