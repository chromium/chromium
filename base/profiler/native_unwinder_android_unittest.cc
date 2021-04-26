// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/native_unwinder_android.h"

#include <sys/mman.h>

#include <inttypes.h>
#include <stdio.h>  // For printf address.
#include <string.h>
#include <algorithm>
#include <iterator>
#include <vector>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/base_profiler_test_support_jni_headers/TestSupport_jni.h"
#include "base/bind.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

extern char __executable_start;

namespace base {

namespace {

// Add a MapInfo with the provided values to |maps|.
void AddMapInfo(uint64_t start,
                uint64_t end,
                uint64_t offset,
                uint64_t flags,
                const std::string& name,
                const std::string& binary_build_id,
                unwindstack::Maps& maps) {
  maps.Add(start, end, offset, flags, name, /* load_bias = */ 0u);
  unwindstack::MapInfo& map_info = **std::prev(maps.end());
  // Yes, this *is* how MapInfo wants this field set. The string is deleted in
  // its destructor.
  map_info.build_id =
      reinterpret_cast<uintptr_t>(new std::string(binary_build_id));
  map_info.elf_offset = map_info.offset;
}

}  // namespace

class TestStackCopierDelegate : public StackCopier::Delegate {
 public:
  void OnStackCopy() override {}
};

std::vector<Frame> CaptureScenario(
    UnwindScenario* scenario,
    ModuleCache* module_cache,
    OnceCallback<void(RegisterContext*, uintptr_t, std::vector<Frame>*)>
        unwind_callback) {
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

            std::move(unwind_callback).Run(&thread_context, stack_top, &sample);
          }));

  return sample;
}

// Checks that the expected information is present in sampled frames.
#if defined(ADDRESS_SANITIZER)
// TODO(https://crbug.com/1147315): Fix, re-enable.
#define MAYBE_PlainFunction DISABLED_PlainFunction
#else
#define MAYBE_PlainFunction PlainFunction
#endif
TEST(NativeUnwinderAndroidTest, MAYBE_PlainFunction) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  std::unique_ptr<unwindstack::Maps> maps = NativeUnwinderAndroid::CreateMaps();
  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();

  ModuleCache module_cache;
  auto unwinder =
      std::make_unique<NativeUnwinderAndroid>(maps.get(), memory.get(), 0);

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
                        EXPECT_EQ(UnwindResult::COMPLETED, result);
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
#if defined(ADDRESS_SANITIZER)
// TODO(https://crbug.com/1147315): Fix, re-enable.
#define MAYBE_Alloca DISABLED_Alloca
#else
#define MAYBE_Alloca Alloca
#endif
TEST(NativeUnwinderAndroidTest, MAYBE_Alloca) {
  UnwindScenario scenario(BindRepeating(&CallWithAlloca));

  std::unique_ptr<unwindstack::Maps> maps = NativeUnwinderAndroid::CreateMaps();
  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();

  ModuleCache module_cache;
  auto unwinder =
      std::make_unique<NativeUnwinderAndroid>(maps.get(), memory.get(), 0);

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
                        EXPECT_EQ(UnwindResult::COMPLETED, result);
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
#if defined(ADDRESS_SANITIZER)
// TODO(https://crbug.com/1147315): Fix, re-enable.
#define MAYBE_OtherLibrary DISABLED_OtherLibrary
#else
#define MAYBE_OtherLibrary OtherLibrary
#endif
TEST(NativeUnwinderAndroidTest, MAYBE_OtherLibrary) {
  NativeLibrary other_library = LoadOtherLibrary();
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library)));

  std::unique_ptr<unwindstack::Maps> maps = NativeUnwinderAndroid::CreateMaps();
  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();

  ModuleCache module_cache;
  auto unwinder =
      std::make_unique<NativeUnwinderAndroid>(maps.get(), memory.get(), 0);

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
                        EXPECT_EQ(UnwindResult::COMPLETED, result);
                      }));

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Check that unwinding is interrupted for excluded modules.
TEST(NativeUnwinderAndroidTest, ExcludeOtherLibrary) {
  NativeLibrary other_library = LoadOtherLibrary();
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library)));

  std::unique_ptr<unwindstack::Maps> maps = NativeUnwinderAndroid::CreateMaps();
  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();
  ModuleCache module_cache;
  unwindstack::MapInfo* other_library_map =
      maps->Find(GetAddressInOtherLibrary(other_library));
  ASSERT_NE(nullptr, other_library_map);
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(
      maps.get(), memory.get(), other_library_map->start);
  unwinder->Initialize(&module_cache);

  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        EXPECT_EQ(UnwindResult::UNRECOGNIZED_FRAME,
                                  unwinder->TryUnwind(thread_context, stack_top,
                                                      sample));
                        EXPECT_FALSE(unwinder->CanUnwindFrom(sample->back()));
                      }));

  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange()});
  ExpectStackDoesNotContain(sample, {scenario.GetSetupFunctionAddressRange(),
                                     scenario.GetOuterFunctionAddressRange()});
}

// Check that unwinding can be resumed after an incomplete unwind.
#if defined(ADDRESS_SANITIZER)
// TODO(https://crbug.com/1147315): Fix, re-enable.
#define MAYBE_ResumeUnwinding DISABLED_ResumeUnwinding
#else
#define MAYBE_ResumeUnwinding ResumeUnwinding
#endif
TEST(NativeUnwinderAndroidTest, MAYBE_ResumeUnwinding) {
  NativeLibrary other_library = LoadOtherLibrary();
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library)));

  std::unique_ptr<unwindstack::Maps> maps = NativeUnwinderAndroid::CreateMaps();
  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();

  // Several unwinders are used to unwind different portion of the stack. Since
  // only 1 unwinder can be registered as a module provider, each unwinder uses
  // a distinct ModuleCache. This tests that NativeUnwinderAndroid can pick up
  // from a state in the middle of the stack. This emulates having
  // NativeUnwinderAndroid work with other unwinders, but doesn't reproduce what
  // happens in production.
  ModuleCache module_cache_for_all;
  auto unwinder_for_all =
      std::make_unique<NativeUnwinderAndroid>(maps.get(), memory.get(), 0);
  unwinder_for_all->Initialize(&module_cache_for_all);

  ModuleCache module_cache_for_native;
  auto unwinder_for_native = std::make_unique<NativeUnwinderAndroid>(
      maps.get(), memory.get(),
      reinterpret_cast<uintptr_t>(&__executable_start));
  unwinder_for_native->Initialize(&module_cache_for_native);

  ModuleCache module_cache_for_chrome;
  unwindstack::MapInfo* other_library_map =
      maps->Find(GetAddressInOtherLibrary(other_library));
  ASSERT_NE(nullptr, other_library_map);
  auto unwinder_for_chrome = std::make_unique<NativeUnwinderAndroid>(
      maps.get(), memory.get(), other_library_map->start);
  unwinder_for_chrome->Initialize(&module_cache_for_chrome);

  std::vector<Frame> sample = CaptureScenario(
      &scenario, &module_cache_for_native,
      BindLambdaForTesting([&](RegisterContext* thread_context,
                               uintptr_t stack_top,
                               std::vector<Frame>* sample) {
        // |unwinder_for_native| unwinds through native frames, but stops at
        // chrome frames. It might not contain SampleAddressRange.
        ASSERT_TRUE(unwinder_for_native->CanUnwindFrom(sample->back()));
        EXPECT_EQ(
            UnwindResult::UNRECOGNIZED_FRAME,
            unwinder_for_native->TryUnwind(thread_context, stack_top, sample));
        EXPECT_FALSE(unwinder_for_native->CanUnwindFrom(sample->back()));

        ExpectStackDoesNotContain(*sample,
                                  {scenario.GetSetupFunctionAddressRange(),
                                   scenario.GetOuterFunctionAddressRange()});
        size_t prior_stack_size = sample->size();

        // |unwinder_for_chrome| unwinds through Chrome frames, but stops at
        // |other_library|. It won't contain SetupFunctionAddressRange.
        ASSERT_TRUE(unwinder_for_chrome->CanUnwindFrom(sample->back()));
        EXPECT_EQ(
            UnwindResult::UNRECOGNIZED_FRAME,
            unwinder_for_chrome->TryUnwind(thread_context, stack_top, sample));
        EXPECT_FALSE(unwinder_for_chrome->CanUnwindFrom(sample->back()));
        EXPECT_LT(prior_stack_size, sample->size());
        ExpectStackContains(*sample, {scenario.GetWaitForSampleAddressRange()});
        ExpectStackDoesNotContain(*sample,
                                  {scenario.GetSetupFunctionAddressRange(),
                                   scenario.GetOuterFunctionAddressRange()});

        // |unwinder_for_all| should complete unwinding through all frames.
        ASSERT_TRUE(unwinder_for_all->CanUnwindFrom(sample->back()));
        EXPECT_EQ(
            UnwindResult::COMPLETED,
            unwinder_for_all->TryUnwind(thread_context, stack_top, sample));
      }));

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

struct JavaTestSupportParams {
  OnceClosure wait_for_sample;
  FunctionAddressRange range;
};

void JNI_TestSupport_InvokeCallbackFunction(JNIEnv* env, jlong context) {
  const void* start_program_counter = GetProgramCounter();

  JavaTestSupportParams* params =
      reinterpret_cast<JavaTestSupportParams*>(context);
  if (!params->wait_for_sample.is_null())
    std::move(params->wait_for_sample).Run();

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();

  params->range = {start_program_counter, end_program_counter};
}

// Checks that java frames can be unwound through.
// Disabled, see: https://crbug.com/1076997
TEST(NativeUnwinderAndroidTest, DISABLED_JavaFunction) {
  auto* build_info = base::android::BuildInfo::GetInstance();
  // Due to varying availability of compiled java unwind tables, unwinding is
  // only expected to succeed on > SDK_VERSION_MARSHMALLOW.
  bool can_always_unwind =
      build_info->sdk_int() > base::android::SDK_VERSION_MARSHMALLOW;

  UnwindScenario scenario(BindLambdaForTesting([](OnceClosure wait_for_sample) {
    JNIEnv* env = base::android::AttachCurrentThread();
    JavaTestSupportParams params{std::move(wait_for_sample), {}};
    base::Java_TestSupport_callWithJavaFunction(
        env, reinterpret_cast<uintptr_t>(&params));
    return params.range;
  }));

  std::unique_ptr<unwindstack::Maps> maps = NativeUnwinderAndroid::CreateMaps();
  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();
  auto unwinder =
      std::make_unique<NativeUnwinderAndroid>(maps.get(), memory.get(), 0);

  ModuleCache module_cache;
  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
                        if (can_always_unwind)
                          EXPECT_EQ(UnwindResult::COMPLETED, result);
                      }));

  // Check that all the modules are valid.
  for (const auto& frame : sample)
    EXPECT_NE(nullptr, frame.module);

  // The stack should contain a full unwind.
  if (can_always_unwind) {
    ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                                 scenario.GetSetupFunctionAddressRange(),
                                 scenario.GetOuterFunctionAddressRange()});
  }
}

TEST(NativeUnwinderAndroidTest, UnwindStackMemoryTest) {
  std::vector<uint8_t> input = {1, 2, 3, 4, 5};
  uintptr_t begin = reinterpret_cast<uintptr_t>(input.data());
  uintptr_t end = reinterpret_cast<uintptr_t>(input.data() + input.size());
  UnwindStackMemoryAndroid memory(begin, end);

  const auto check_read_fails = [&](uintptr_t addr, size_t size) {
    std::vector<uint8_t> output(size);
    EXPECT_EQ(0U, memory.Read(addr, output.data(), size));
  };
  const auto check_read_succeeds = [&](uintptr_t addr, size_t size) {
    std::vector<uint8_t> output(size);
    EXPECT_EQ(size, memory.Read(addr, output.data(), size));
    EXPECT_EQ(
        0, memcmp(reinterpret_cast<const uint8_t*>(addr), output.data(), size));
  };

  check_read_fails(begin - 1, 1);
  check_read_fails(begin - 1, 2);
  check_read_fails(end, 1);
  check_read_fails(end, 2);
  check_read_fails(end - 1, 2);

  check_read_succeeds(begin, 1);
  check_read_succeeds(begin, 5);
  check_read_succeeds(end - 1, 1);
}

// Checks the debug basename is the whole name for a non-ELF module.
TEST(NativeUnwinderAndroidTest, ModuleDebugBasenameForNonElf) {
  unwindstack::Maps maps;

  AddMapInfo(0x1000u, 0x2000u, 0u, PROT_READ | PROT_EXEC, "[foo / bar]", {0xAA},
             maps);

  ModuleCache module_cache;

  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();
  auto unwinder =
      std::make_unique<NativeUnwinderAndroid>(&maps, memory.get(), 0);
  unwinder->Initialize(&module_cache);

  const ModuleCache::Module* module = module_cache.GetModuleForAddress(0x1000u);

  ASSERT_TRUE(module);
  EXPECT_EQ("[foo / bar]", module->GetDebugBasename().value());
}

// Checks that modules are only created for executable memory regions.
TEST(NativeUnwinderAndroidTest, ModulesCreatedOnlyForExecutableRegions) {
  unwindstack::Maps maps;
  AddMapInfo(0x1000u, 0x2000u, 0u, PROT_READ | PROT_EXEC, "[a]", {0xAA}, maps);
  AddMapInfo(0x2000u, 0x3000u, 0u, PROT_READ, "[b]", {0xAB}, maps);
  AddMapInfo(0x3000u, 0x4000u, 0u, PROT_READ | PROT_EXEC, "[c]", {0xAC}, maps);

  std::unique_ptr<unwindstack::Memory> memory =
      NativeUnwinderAndroid::CreateProcessMemory();

  ModuleCache module_cache;
  auto unwinder =
      std::make_unique<NativeUnwinderAndroid>(&maps, memory.get(), 0);
  unwinder->Initialize(&module_cache);

  const ModuleCache::Module* module1 =
      module_cache.GetModuleForAddress(0x1000u);
  const ModuleCache::Module* module2 =
      module_cache.GetModuleForAddress(0x2000u);
  const ModuleCache::Module* module3 =
      module_cache.GetModuleForAddress(0x3000u);

  ASSERT_TRUE(module1);
  EXPECT_EQ(0x1000u, module1->GetBaseAddress());
  EXPECT_EQ(nullptr, module2);
  ASSERT_TRUE(module3);
  EXPECT_EQ(0x3000u, module3->GetBaseAddress());
}

}  // namespace base
