// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/libunwindstack_unwinder_android.h"

#include <sys/mman.h>

#include <inttypes.h>
#include <string.h>
#include <vector>

#include "base/android/build_info.h"
#include "base/functional/bind.h"
#include "base/native_library.h"
#include "base/path_service.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampling_profiler_java_test_util.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "stack_sampling_profiler_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {
class TestStackCopierDelegate : public StackCopier::Delegate {
 public:
  void OnStackCopy() override {}
};

std::vector<Frame> CaptureScenario(
    UnwindScenario* scenario,
    ModuleCache* module_cache,
    OnceCallback<void(UnwinderStateCapture*,
                      RegisterContext*,
                      uintptr_t,
                      std::vector<Frame>*)> unwind_callback) {
  std::vector<Frame> sample;
  WithTargetThread(
      scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            auto thread_delegate =
                ThreadDelegatePosix::Create(target_thread_token);
            ASSERT_TRUE(thread_delegate);
            auto stack_copier =
                std::make_unique<StackCopierSignal>(std::move(thread_delegate));
            std::unique_ptr<StackBuffer> stack_buffer =
                StackSampler::CreateStackBuffer();

            RegisterContext thread_context;
            uintptr_t stack_top;
            TimeTicks timestamp;
            TestStackCopierDelegate delegate;
            bool success =
                stack_copier->CopyStack(stack_buffer.get(), &stack_top,
                                        &timestamp, &thread_context, &delegate);
            ASSERT_TRUE(success);

            sample.emplace_back(
                RegisterContextInstructionPointer(&thread_context),
                module_cache->GetModuleForAddress(
                    RegisterContextInstructionPointer(&thread_context)));

            std::move(unwind_callback)
                .Run(nullptr, &thread_context, stack_top, &sample);
          }));

  return sample;
}
}  // namespace

// Checks that the expected information is present in sampled frames.
TEST(LibunwindstackUnwinderAndroidTest, PlainFunction) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  ModuleCache module_cache;
  auto unwinder = std::make_unique<LibunwindstackUnwinderAndroid>();

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample = CaptureScenario(
      &scenario, &module_cache,
      BindLambdaForTesting([&](UnwinderStateCapture* capture_state,
                               RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<Frame>* sample) {
        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
        UnwindResult result = unwinder->TryUnwind(capture_state, thread_context,
                                                  stack_top, sample);
        EXPECT_EQ(UnwindResult::kCompleted, result);
      }));

  // Check that all the modules are valid.
  for (const auto& frame : sample)
    EXPECT_NE(nullptr, frame.module);

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that the unwinder handles stacks containing dynamically-allocated
// stack memory.
TEST(LibunwindstackUnwinderAndroidTest, Alloca) {
  UnwindScenario scenario(BindRepeating(&CallWithAlloca));

  ModuleCache module_cache;
  auto unwinder = std::make_unique<LibunwindstackUnwinderAndroid>();

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample = CaptureScenario(
      &scenario, &module_cache,
      BindLambdaForTesting([&](UnwinderStateCapture* capture_state,
                               RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<Frame>* sample) {
        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
        UnwindResult result = unwinder->TryUnwind(capture_state, thread_context,
                                                  stack_top, sample);
        EXPECT_EQ(UnwindResult::kCompleted, result);
      }));

  // Check that all the modules are valid.
  for (const auto& frame : sample)
    EXPECT_NE(nullptr, frame.module);

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that a stack that runs through another library produces a stack with
// the expected functions.
TEST(LibunwindstackUnwinderAndroidTest, OtherLibrary) {
  NativeLibrary other_library = LoadOtherLibrary();
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library)));

  ModuleCache module_cache;
  auto unwinder = std::make_unique<LibunwindstackUnwinderAndroid>();

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample = CaptureScenario(
      &scenario, &module_cache,
      BindLambdaForTesting([&](UnwinderStateCapture* capture_state,
                               RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<Frame>* sample) {
        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
        UnwindResult result = unwinder->TryUnwind(capture_state, thread_context,
                                                  stack_top, sample);
        EXPECT_EQ(UnwindResult::kCompleted, result);
      }));

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that java frames can be unwound through and have function names.
TEST(LibunwindstackUnwinderAndroidTest, JavaFunction) {
  auto* build_info = base::android::BuildInfo::GetInstance();
  // Due to varying availability of compiled/JITed java unwind tables, unwinding
  // is only expected to reliably succeed on Android P+
  // https://android.googlesource.com/platform/system/unwinding/+/refs/heads/master/libunwindstack/AndroidVersions.md#android-9-pie_api-level-28
  // The libunwindstack doc mentions in Android 9 it got the support for
  // unwinding through JIT'd frames.
  bool can_unwind = build_info->sdk_int() >= base::android::SDK_VERSION_P;
  if (!can_unwind) {
    GTEST_SKIP() << "Unwind info is not available on older version of Android";
  }

  UnwindScenario scenario(base::BindRepeating(callWithJavaFunction));

  auto unwinder = std::make_unique<LibunwindstackUnwinderAndroid>();

  ModuleCache module_cache;
  unwinder->Initialize(&module_cache);
  const std::vector<Frame> sample = CaptureScenario(
      &scenario, &module_cache,
      BindLambdaForTesting([&](UnwinderStateCapture* capture_state,
                               RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<Frame>* sample) {
        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
        UnwindResult result = unwinder->TryUnwind(capture_state, thread_context,
                                                  stack_top, sample);
        EXPECT_EQ(UnwindResult::kCompleted, result);
      }));

  // Check that all the modules are valid.
  for (const auto& frame : sample) {
    EXPECT_NE(frame.module, nullptr);
  }

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// TODO(b/269239545): Delete or re-enable (and enable updatable maps) this test.
TEST(LibunwindstackUnwinderAndroidTest,
     DISABLED_ReparsesMapsOnNewDynamicLibraryLoad) {
  // The current version of /proc/self/maps is used to create
  // memory_regions_map_ object.
  auto unwinder = std::make_unique<LibunwindstackUnwinderAndroid>();
  ModuleCache module_cache;
  unwinder->Initialize(&module_cache);

  // Dynamically loading a library should update maps and a reparse is required
  // to actually unwind through functions involving this library.
  NativeLibrary dynamic_library =
      LoadTestLibrary("base_profiler_reparsing_test_support_library");
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(dynamic_library)));

  auto sample = CaptureScenario(
      &scenario, &module_cache,
      BindLambdaForTesting([&](UnwinderStateCapture* capture_state,
                               RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<Frame>* sample) {
        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
        UnwindResult result = unwinder->TryUnwind(capture_state, thread_context,
                                                  stack_top, sample);
        EXPECT_EQ(UnwindResult::kCompleted, result);
      }));

  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

}  // namespace base
