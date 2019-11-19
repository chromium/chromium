// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/buildflags.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_log.h"
#include "build/build_config.h"
#include "chrome/browser/profiling_host/profiling_process_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/heap_profiling/test_driver.h"
#include "components/services/heap_profiling/public/cpp/settings.h"
#include "components/services/heap_profiling/public/cpp/switches.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/zlib.h"

// Some builds don't support memlog in which case the tests won't function.
#if BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace heap_profiling {

struct TestParam {
  Mode mode;
  mojom::StackMode stack_mode;
  bool start_profiling_with_command_line_flag;
  bool should_sample;
  bool sample_everything;
};

class MemlogBrowserTest : public InProcessBrowserTest,
                          public testing::WithParamInterface<TestParam> {
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    if (!GetParam().start_profiling_with_command_line_flag)
      return;

    if (GetParam().mode == Mode::kAllRenderers) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogMode,
                                      heap_profiling::kMemlogModeAllRenderers);
    } else if (GetParam().mode == Mode::kAll) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogMode,
                                      heap_profiling::kMemlogModeAll);
    } else {
      NOTREACHED();
    }

    if (!GetParam().should_sample) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogSamplingRate, "1");
    }

    if (GetParam().stack_mode == mojom::StackMode::PSEUDO) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogStackMode,
                                      heap_profiling::kMemlogStackModePseudo);
    } else if (GetParam().stack_mode ==
               mojom::StackMode::NATIVE_WITH_THREAD_NAMES) {
      command_line->AppendSwitchASCII(
          heap_profiling::kMemlogStackMode,
          heap_profiling::kMemlogStackModeNativeWithThreadNames);
    } else if (GetParam().stack_mode ==
               mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogStackMode,
                                      heap_profiling::kMemlogStackModeNative);
    } else if (GetParam().stack_mode == mojom::StackMode::MIXED) {
      command_line->AppendSwitchASCII(heap_profiling::kMemlogStackMode,
                                      heap_profiling::kMemlogStackModeMixed);
    } else {
      NOTREACHED();
    }
  }
};

// Ensure invocations via TracingController can generate a valid JSON file with
// expected data.
// TODO(crbug.com/843467): Disabled due to flakiness.
IN_PROC_BROWSER_TEST_P(MemlogBrowserTest, DISABLED_EndToEnd) {
  LOG(INFO) << "Memlog mode: " << static_cast<int>(GetParam().mode);
  LOG(INFO) << "Memlog stack mode: " << static_cast<int>(GetParam().stack_mode);
  LOG(INFO) << "Started via command line flag: "
            << GetParam().start_profiling_with_command_line_flag;
  LOG(INFO) << "Should sample: " << GetParam().should_sample;
  LOG(INFO) << "Sample everything: " << GetParam().sample_everything;
  TestDriver driver;
  TestDriver::Options options;
  options.mode = GetParam().mode;
  options.stack_mode = GetParam().stack_mode;
  options.profiling_already_started =
      GetParam().start_profiling_with_command_line_flag;
  options.should_sample = GetParam().should_sample;
  options.sample_everything = GetParam().sample_everything;

  EXPECT_TRUE(driver.RunTest(options));
}
// TODO(ajwong): Test what happens if profiling process crashes.
// http://crbug.com/780955

std::vector<TestParam> GetParams() {
  std::vector<TestParam> params;
  std::vector<Mode> dynamic_start_modes{Mode::kNone, Mode::kMinimal,
                                        Mode::kBrowser, Mode::kGpu};

  std::vector<mojom::StackMode> stack_modes{
      mojom::StackMode::MIXED, mojom::StackMode::PSEUDO,
      mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES,
      mojom::StackMode::NATIVE_WITH_THREAD_NAMES};

  for (const auto& stack_mode : stack_modes) {
    for (const auto& mode : dynamic_start_modes) {
      params.push_back(
          {mode, stack_mode, false /* start_profiling_with_command_line_flag */,
           true /* should_sample */, false /* sample_everything*/});
    }

// For unknown reasons, renderer profiling has become flaky on ChromeOS. This is
// likely happening because the renderers are never being given the signal to
// start profiling. It's unclear why this happens. https://crbug.com/843843.
// https://crbug.com/843467.
#if !defined(OS_CHROMEOS)
    // Non-browser processes must be profiled with a command line flag, since
    // otherwise, profiling will start after the relevant processes have been
    // created, thus that process will be not be profiled.
    std::vector<Mode> command_line_start_modes{Mode::kAll, Mode::kAllRenderers};
    for (const auto& mode : command_line_start_modes) {
      params.push_back(
          {mode, stack_mode, true /* start_profiling_with_command_line_flag */,
           true /* should_sample */, false /* sample_everything*/});
    }
#endif  // defined(OS_CHROMEOS)
  }

  return params;
}

INSTANTIATE_TEST_SUITE_P(Memlog,
                         MemlogBrowserTest,
                         ::testing::ValuesIn(GetParams()));

}  // namespace heap_profiling

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
