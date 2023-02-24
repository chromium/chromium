// Copyright 2020 The Chromium Authors
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
#include "base/functional/bind.h"
#include "base/profiler/native_unwinder_android_map_delegate.h"
#include "base/profiler/native_unwinder_android_memory_regions_map.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampling_profiler_java_test_util.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Maps.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Memory.h"

extern char __executable_start;

namespace base {

namespace {

// Add a MapInfo with the provided values to |maps|.
void AddMapInfo(uint64_t start,
                uint64_t end,
                uint64_t offset,
                uint64_t flags,
                std::string name,
                const std::string& binary_build_id,
                unwindstack::Maps& maps) {
  maps.Add(start, end, offset, flags, name, /* load_bias = */ 0u);
  unwindstack::MapInfo& map_info = **std::prev(maps.end());
  map_info.SetBuildID(std::move(name));
  map_info.set_elf_offset(map_info.offset());
}

class NativeUnwinderAndroidMemoryRegionsMapForTesting
    : public NativeUnwinderAndroidMemoryRegionsMap {
 public:
  NativeUnwinderAndroidMemoryRegionsMapForTesting(unwindstack::Maps* maps,
                                                  unwindstack::Memory* memory)
      : maps_(maps), memory_(memory) {}
  unwindstack::Maps* GetMaps() override { return maps_; }
  unwindstack::Memory* GetMemory() override { return memory_; }
  // This function is not expected to be called within the unittest, as
  // `NativeUnwinderAndroidMemoryRegionsMapForTesting` does not own
  // `unwindstack::Memory`.
  std::unique_ptr<unwindstack::Memory> TakeMemory() override { return nullptr; }

 private:
  unwindstack::Maps* maps_;
  unwindstack::Memory* memory_;
};

class NativeUnwinderAndroidMapDelegateForTesting
    : public NativeUnwinderAndroidMapDelegate {
 public:
  explicit NativeUnwinderAndroidMapDelegateForTesting(
      std::unique_ptr<NativeUnwinderAndroidMemoryRegionsMap> memory_regions_map)
      : memory_regions_map_(std::move(memory_regions_map)) {}

  NativeUnwinderAndroidMemoryRegionsMap* GetMapReference() override {
    acquire_count_++;
    return memory_regions_map_.get();
  }
  void ReleaseMapReference() override { release_count_++; }

  uint32_t acquire_count() { return acquire_count_; }
  uint32_t release_count() { return release_count_; }

 private:
  const std::unique_ptr<NativeUnwinderAndroidMemoryRegionsMap>
      memory_regions_map_;

  uint32_t acquire_count_ = 0u;
  uint32_t release_count_ = 0u;
};

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
// TODO(https://crbug.com/1147315): After fix, re-enable on all ASAN bots.
// TODO(https://crbug.com/1368981): After fix, re-enable on all bots except
// if defined(ADDRESS_SANITIZER).
TEST(NativeUnwinderAndroidTest, DISABLED_PlainFunction) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));
  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());

  ModuleCache module_cache;
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
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
// TODO(https://crbug.com/1147315): After fix, re-enable on all ASAN bots.
// TODO(https://crbug.com/1368981): After fix, re-enable on all bots except
// if defined(ADDRESS_SANITIZER).
TEST(NativeUnwinderAndroidTest, DISABLED_Alloca) {
  UnwindScenario scenario(BindRepeating(&CallWithAlloca));

  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());

  ModuleCache module_cache;
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
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
// TODO(https://crbug.com/1147315): After fix, re-enable on all ASAN bots.
// TODO(https://crbug.com/1368981): After fix, re-enable on all bots except
// if defined(ADDRESS_SANITIZER).
TEST(NativeUnwinderAndroidTest, DISABLED_OtherLibrary) {
  NativeLibrary other_library = LoadOtherLibrary();
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library)));

  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());
  ModuleCache module_cache;
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);

  unwinder->Initialize(&module_cache);
  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        UnwindResult result = unwinder->TryUnwind(
                            thread_context, stack_top, sample);
                        EXPECT_EQ(UnwindResult::kCompleted, result);
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

  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());
  ModuleCache module_cache;
  unwindstack::MapInfo* other_library_map =
      map_delegate.GetMapReference()
          ->GetMaps()
          ->Find(GetAddressInOtherLibrary(other_library))
          .get();
  ASSERT_NE(nullptr, other_library_map);
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(
      other_library_map->start(), &map_delegate);
  unwinder->Initialize(&module_cache);

  std::vector<Frame> sample =
      CaptureScenario(&scenario, &module_cache,
                      BindLambdaForTesting([&](RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               std::vector<Frame>* sample) {
                        ASSERT_TRUE(unwinder->CanUnwindFrom(sample->back()));
                        EXPECT_EQ(UnwindResult::kUnrecognizedFrame,
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

  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());

  // Several unwinders are used to unwind different portion of the stack. Since
  // only 1 unwinder can be registered as a module provider, each unwinder uses
  // a distinct ModuleCache. This tests that NativeUnwinderAndroid can pick up
  // from a state in the middle of the stack. This emulates having
  // NativeUnwinderAndroid work with other unwinders, but doesn't reproduce what
  // happens in production.
  ModuleCache module_cache_for_all;
  auto unwinder_for_all =
      std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);
  unwinder_for_all->Initialize(&module_cache_for_all);

  ModuleCache module_cache_for_native;
  auto unwinder_for_native = std::make_unique<NativeUnwinderAndroid>(
      reinterpret_cast<uintptr_t>(&__executable_start), &map_delegate);
  unwinder_for_native->Initialize(&module_cache_for_native);

  ModuleCache module_cache_for_chrome;
  unwindstack::MapInfo* other_library_map =
      map_delegate.GetMapReference()
          ->GetMaps()
          ->Find(GetAddressInOtherLibrary(other_library))
          .get();
  ASSERT_NE(nullptr, other_library_map);
  auto unwinder_for_chrome = std::make_unique<NativeUnwinderAndroid>(
      other_library_map->start(), &map_delegate);
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
            UnwindResult::kUnrecognizedFrame,
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
            UnwindResult::kUnrecognizedFrame,
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
            UnwindResult::kCompleted,
            unwinder_for_all->TryUnwind(thread_context, stack_top, sample));
      }));

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that java frames can be unwound through.
// Disabled, see: https://crbug.com/1076997
TEST(NativeUnwinderAndroidTest, DISABLED_JavaFunction) {
  auto* build_info = base::android::BuildInfo::GetInstance();
  // Due to varying availability of compiled java unwind tables, unwinding is
  // only expected to succeed on > SDK_VERSION_MARSHMALLOW.
  bool can_always_unwind =
      build_info->sdk_int() > base::android::SDK_VERSION_MARSHMALLOW;

  UnwindScenario scenario(base::BindRepeating(callWithJavaFunction));

  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);

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
                          EXPECT_EQ(UnwindResult::kCompleted, result);
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

  auto memory_regions_map = NativeUnwinderAndroid::CreateMemoryRegionsMap();
  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      std::make_unique<NativeUnwinderAndroidMemoryRegionsMapForTesting>(
          &maps, memory_regions_map->GetMemory()));
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);
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

  auto memory_regions_map = NativeUnwinderAndroid::CreateMemoryRegionsMap();
  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      std::make_unique<NativeUnwinderAndroidMemoryRegionsMapForTesting>(
          &maps, memory_regions_map->GetMemory()));
  ModuleCache module_cache;
  auto unwinder = std::make_unique<NativeUnwinderAndroid>(0, &map_delegate);
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

TEST(NativeUnwinderAndroidTest,
     AcquireAndReleaseMemoryRegionsMapThroughMapDelegate) {
  NativeUnwinderAndroidMapDelegateForTesting map_delegate(
      NativeUnwinderAndroid::CreateMemoryRegionsMap());

  {
    ModuleCache module_cache;
    auto unwinder = std::make_unique<NativeUnwinderAndroid>(
        /* exclude_module_with_base_address= */ 0, &map_delegate);
    unwinder->Initialize(&module_cache);
    EXPECT_EQ(1u, map_delegate.acquire_count());
    EXPECT_EQ(0u, map_delegate.release_count());
  }

  EXPECT_EQ(1u, map_delegate.acquire_count());
  EXPECT_EQ(1u, map_delegate.release_count());
}

}  // namespace base
