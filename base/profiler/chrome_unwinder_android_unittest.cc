// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/chrome_unwinder_android.h"

#include "base/android/library_loader/anchor_functions.h"
#include "base/files/file_util.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// Input is generated from the CFI file:
// STACK CFI INIT 100 100
// STACK CFI 1010 .cfa: sp 8 + .ra: .cfa -4 + ^
const uint16_t cfi_data[] = {
    // UNW_INDEX size
    0x02,
    0x0,
    // UNW_INDEX function_addresses (4 byte rows).
    0x100,
    0x0,
    0x200,
    0x0,
    // UNW_INDEX entry_data_indices (2 byte rows).
    0x0,
    0xffff,
    // UNW_DATA table.
    0x1,
    0x10,
    0x9,
};

class TestModule : public ModuleCache::Module {
 public:
  TestModule(uintptr_t base_address,
             size_t size,
             const std::string& build_id = "TestModule")
      : base_address_(base_address), size_(size), build_id_(build_id) {}

  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return build_id_; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return true; }

 private:
  const uintptr_t base_address_;
  const size_t size_;
  const std::string build_id_;
};

// Utility function to add a single native module during test setup. Returns
// a pointer to the provided module.
const ModuleCache::Module* AddNativeModule(
    ModuleCache* cache,
    std::unique_ptr<const ModuleCache::Module> module) {
  const ModuleCache::Module* module_ptr = module.get();
  cache->AddCustomNativeModule(std::move(module));
  return module_ptr;
}

ArmCFITable::FrameEntry MakeFrameEntry(uint16_t cfa_offset,
                                       uint16_t ra_offset) {
  return ArmCFITable::FrameEntry{
      static_cast<uint16_t>(cfa_offset * sizeof(uintptr_t)),
      static_cast<uint16_t>(ra_offset * sizeof(uintptr_t))};
}

}  // namespace

bool operator==(const Frame& a, const Frame& b) {
  return a.instruction_pointer == b.instruction_pointer && a.module == b.module;
}

// Tests unwind step under normal operation.
TEST(ChromeUnwinderAndroidTest, Step) {
  const std::vector<uintptr_t> stack_buffer = {
      0xFFFF, 0xFFFF, 0xFFFF, 0x1111, 0xFFFF, 0x2222,
  };
  const uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0xBEEF;
  RegisterContextStackPointer(&context) =
      reinterpret_cast<uintptr_t>(stack_buffer.data());

  EXPECT_TRUE(ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                                    MakeFrameEntry(4, 1)));
  EXPECT_EQ(RegisterContextInstructionPointer(&context), 0x1111U);
  EXPECT_EQ(RegisterContextStackPointer(&context),
            reinterpret_cast<uintptr_t>(stack_buffer.data() + 4));

  EXPECT_TRUE(ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                                    MakeFrameEntry(1, 0)));
  EXPECT_EQ(RegisterContextInstructionPointer(&context), 0x2222U);
  EXPECT_EQ(RegisterContextStackPointer(&context),
            reinterpret_cast<uintptr_t>(stack_buffer.data() + 5));
}

// Tests unwind step using immediate return address.
TEST(ChromeUnwinderAndroidTest, StepImmediate) {
  const std::vector<uintptr_t> stack_buffer = {0xFFFF, 0xFFFF};
  const uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0xBEEF;
  RegisterContextStackPointer(&context) =
      reinterpret_cast<uintptr_t>(stack_buffer.data());
  context.arm_lr = 0x4444;

  EXPECT_TRUE(ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                                    MakeFrameEntry(0, 0)));
  EXPECT_EQ(RegisterContextInstructionPointer(&context), 0x4444U);
  EXPECT_EQ(RegisterContextStackPointer(&context),
            reinterpret_cast<uintptr_t>(stack_buffer.data()));
}

// Tests that unwinding fails if immediate return address is the current
// instruction.
TEST(ChromeUnwinderAndroidTest, StepImmediateFail) {
  const std::vector<uintptr_t> stack_buffer = {0xFFFF, 0xFFFF};
  const uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());

  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0x1111;
  RegisterContextStackPointer(&context) =
      reinterpret_cast<uintptr_t>(stack_buffer.data());
  context.arm_lr = 0x1111;

  EXPECT_FALSE(ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                                     MakeFrameEntry(0, 0)));
}

// Tests that unwinding fails if stack is invalid.
TEST(ChromeUnwinderAndroidTest, StepInvalidStack) {
  const std::vector<uintptr_t> stack_buffer = {0xFFFF};

  const uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());

  RegisterContext context;
  EXPECT_CHECK_DEATH({
    RegisterContextStackPointer(&context) = 0;

    ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                          MakeFrameEntry(1, 0));
  });

  EXPECT_CHECK_DEATH({
    RegisterContextStackPointer(&context) = reinterpret_cast<uintptr_t>(
        stack_buffer.data() + stack_buffer.size() + 1);

    ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                          MakeFrameEntry(1, 0));
  });
}

// Tests that unwinding fails if the frame entry is out of bounds.
TEST(ChromeUnwinderAndroidTest, StepOutOfBounds) {
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0xBEEF;
  constexpr uintptr_t kOverflowOffset = 8;
  constexpr size_t kStackSize = 4;
  // It's fine to use a fake stack pointer since the stack won't be
  // dereferenced. Purposely underflow so that sp + |kOverflowOffset| overflows.
  RegisterContextStackPointer(&context) = 0 - kOverflowOffset;
  const uintptr_t stack_top =
      RegisterContextStackPointer(&context) + kStackSize;

  // ra_offset exceeds cfa_offset.
  EXPECT_FALSE(ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                                     MakeFrameEntry(1, 2)));
  EXPECT_FALSE(ChromeUnwinderAndroid::StepForTesting(
      &context, stack_top, MakeFrameEntry(1, kOverflowOffset)));

  // cfa_offset exceeds |stack_top|.
  EXPECT_FALSE(ChromeUnwinderAndroid::StepForTesting(
      &context, stack_top, MakeFrameEntry(kStackSize, 0)));

  // sp + cfa_offset overflows.
  EXPECT_FALSE(ChromeUnwinderAndroid::StepForTesting(
      &context, stack_top, MakeFrameEntry(kOverflowOffset, 0)));
}

TEST(ChromeUnwinderAndroidTest, StepUnderflows) {
  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0xBEEF;
  // It's fine to use a fake stack pointer since the stack won't be
  // dereferenced.
  RegisterContextStackPointer(&context) = 2;
  const uintptr_t stack_top = RegisterContextStackPointer(&context) + 4;

  // sp + cfa_offset - ra_offset underflows.
  EXPECT_FALSE(ChromeUnwinderAndroid::StepForTesting(&context, stack_top,
                                                     MakeFrameEntry(1, 4)));
}

TEST(ChromeUnwinderAndroidTest, CanUnwindFrom) {
  auto cfi_table = ArmCFITable::Parse(
      {reinterpret_cast<const uint8_t*>(cfi_data), sizeof(cfi_data)});

  auto chrome_module =
      std::make_unique<TestModule>(0x1000, 0x500, "ChromeModule");
  auto non_chrome_module =
      std::make_unique<TestModule>(0x2000, 0x500, "OtherModule");

  ModuleCache module_cache;
  ChromeUnwinderAndroid unwinder(cfi_table.get(),
                                 chrome_module->GetBaseAddress());
  unwinder.Initialize(&module_cache);

  EXPECT_TRUE(unwinder.CanUnwindFrom({0x1100, chrome_module.get()}));
  EXPECT_FALSE(unwinder.CanUnwindFrom({0x2100, non_chrome_module.get()}));
}

TEST(ChromeUnwinderAndroidTest, TryUnwind) {
  auto cfi_table = ArmCFITable::Parse(
      {reinterpret_cast<const uint8_t*>(cfi_data), sizeof(cfi_data)});

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(0x1000, 0x500));

  ChromeUnwinderAndroid unwinder(cfi_table.get(),
                                 chrome_module->GetBaseAddress());
  unwinder.Initialize(&module_cache);

  std::vector<uintptr_t> stack_buffer = {
      0xFFFF,
      // .cfa: sp 8 + .ra: .cfa -4 + ^
      0x2000,
      0xFFFF,
  };
  uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());

  std::vector<Frame> stack;
  stack.emplace_back(0x1100, chrome_module);

  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0x1100;
  RegisterContextStackPointer(&context) =
      reinterpret_cast<uintptr_t>(stack_buffer.data());
  context.arm_lr = 0x11AA;

  EXPECT_EQ(UnwindResult::UNRECOGNIZED_FRAME,
            unwinder.TryUnwind(&context, stack_top, &stack));
  EXPECT_EQ(std::vector<Frame>({{0x1100, chrome_module},
                                {0x11AA, chrome_module},
                                {0x2000, nullptr}}),
            stack);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindAbort) {
  auto cfi_table = ArmCFITable::Parse(
      {reinterpret_cast<const uint8_t*>(cfi_data), sizeof(cfi_data)});
  ASSERT_TRUE(cfi_table);

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(0x1000, 0x500));

  ChromeUnwinderAndroid unwinder(cfi_table.get(),
                                 chrome_module->GetBaseAddress());
  unwinder.Initialize(&module_cache);

  std::vector<uintptr_t> stack_buffer = {
      0xFFFF,
  };
  uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());

  std::vector<Frame> stack;
  stack.emplace_back(0x1100, chrome_module);

  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0x1100;
  RegisterContextStackPointer(&context) =
      reinterpret_cast<uintptr_t>(stack_buffer.data());
  context.arm_lr = 0x1100;

  // Aborted because ra == pc.
  EXPECT_EQ(UnwindResult::ABORTED,
            unwinder.TryUnwind(&context, stack_top, &stack));
  EXPECT_EQ(std::vector<Frame>({{0x1100, chrome_module}}), stack);
}

TEST(ChromeUnwinderAndroidTest, TryUnwindNoData) {
  auto cfi_table = ArmCFITable::Parse(
      {reinterpret_cast<const uint8_t*>(cfi_data), sizeof(cfi_data)});

  ModuleCache module_cache;
  const ModuleCache::Module* chrome_module = AddNativeModule(
      &module_cache, std::make_unique<TestModule>(0x1000, 0x500));

  ChromeUnwinderAndroid unwinder(cfi_table.get(),
                                 chrome_module->GetBaseAddress());
  unwinder.Initialize(&module_cache);

  std::vector<uintptr_t> stack_buffer = {0xFFFF};

  uintptr_t stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer.data() + stack_buffer.size());

  std::vector<Frame> stack;
  stack.emplace_back(0x1200, chrome_module);

  RegisterContext context;
  RegisterContextInstructionPointer(&context) = 0xBEEF;
  RegisterContextStackPointer(&context) =
      reinterpret_cast<uintptr_t>(stack_buffer.data());
  context.arm_lr = 0x12AA;

  // Unwinding will first use arm_lr as fallback because there's no unwind info
  // for the instruction pointer, and then abort.
  EXPECT_EQ(UnwindResult::ABORTED,
            unwinder.TryUnwind(&context, stack_top, &stack));
  EXPECT_EQ(
      std::vector<Frame>({{0x1200, chrome_module}, {0x12AA, chrome_module}}),
      stack);
}

}  // namespace base
