// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugging_buildflags.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/app/chrome_crash_reporter_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/allocation_recorder/testing/crash_verification.h"
#include "components/memory_system/memory_system_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
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

class AllocationRecorderBrowserTest : public PlatformBrowserTest,
                                      public testing::WithParamInterface<bool> {
 public:
  AllocationRecorderBrowserTest() : atr_enabled_(GetParam()) {
    if (atr_enabled_) {
      scoped_feature_list_.InitWithFeaturesAndParameters(
          /*enabled_features=*/
          {{memory_system::features::kAllocationTraceRecorder,
            {{"atr_force_all_processes", "true"}}}},
          /*disabled_features=*/
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              memory_system::features::kAllocationTraceRecorder});
    }
  }

  ~AllocationRecorderBrowserTest() override = default;

  bool AllocationTraceRecorderEnabled() const { return atr_enabled_; }

  void CrashRendererProcess() {
    const GURL kCrashUrl("chrome://crash");
    auto* const web_contents = chrome_test_utils::GetActiveWebContents(this);
    const content::ScopedAllowRendererCrashes allow_renderer_crashes(
        web_contents);
    ASSERT_FALSE(content::NavigateToURL(web_contents, kCrashUrl))
        << "Loading crash url didn't crash the browser. url='" << kCrashUrl
        << '\'';
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool atr_enabled_;
};

INSTANTIATE_TEST_SUITE_P(AllocationRecorderBrowserTests,
                         AllocationRecorderBrowserTest,
                         testing::Bool());

IN_PROC_BROWSER_TEST_P(AllocationRecorderBrowserTest,
                       VerifyCrashreportIncludesRecorder) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath crashpad_database_path = temp_dir.GetPath();
  base::ScopedPathOverride path_holder(chrome::DIR_CRASH_DUMPS,
                                       crashpad_database_path);

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  allocation_recorder::testing::
      VerifyCrashCreatesCrashpadReportWithAllocationRecorderStream(
          crashpad_database_path,
          base::BindOnce(&AllocationRecorderBrowserTest::CrashRendererProcess,
                         base::Unretained(this)),
          base::BindOnce(&allocation_recorder::testing::VerifyPayload,
                         AllocationTraceRecorderEnabled()));
#else
  allocation_recorder::testing::
      VerifyCrashCreatesCrashpadReportWithoutAllocationRecorderStream(
          crashpad_database_path,
          base::BindOnce(&AllocationRecorderBrowserTest::CrashRendererProcess,
                         base::Unretained(this)));
#endif
}
