// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/frame_pointer_unwinder.h"

#include <memory>

#include "base/profiler/module_cache.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/unwinder.h"
#include "build/buildflag.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_APPLE)
#include "base/mac/mac_util.h"
#endif

namespace base {

namespace {

constexpr uintptr_t kModuleStart = 0x1000;
constexpr size_t kModuleSize = 0x1000;
constexpr uintptr_t kNonNativeModuleStart = 0x4000;

// Used to construct test stacks. If `relative` is true, the value should be the
// address `offset` positions from the bottom of the stack (at 8-byte alignment)
// Otherwise, `offset` is added to the stack as an absolute address/value.
// For example, when creating a stack with bottom 0x2000, {false, 0xf00d} will
// become 0xf00d, and {true, 0x3} will become 0x2018.
struct StackEntrySpec {
  bool relative;
  uintptr_t offset;
};

// Enables constructing a stack buffer that has pointers to itself
// and provides convenience methods for calling the unwinder.
struct InputStack {
  explicit InputStack(const std::vector<StackEntrySpec>& offsets)
      : buffer(offsets.size()) {
    size_t size = offsets.size();
    for (size_t i = 0; i < size; ++i) {
      auto spec = offsets[i];
      if (spec.relative) {
        buffer[i] = bottom() + (spec.offset * sizeof(uintptr_t));
      } else {
        buffer[i] = spec.offset;
      }
    }
  }
  uintptr_t bottom() const {
    return reinterpret_cast<uintptr_t>(buffer.data());
  }
  uintptr_t top() const { return bottom() + buffer.size() * sizeof(uintptr_t); }

 private:
  std::vector<uintptr_t> buffer;
};

}  // namespace

class FramePointerUnwinderTest : public testing::Test {
 protected:
  FramePointerUnwinderTest() {
#if BUILDFLAG(IS_APPLE)
    if (__builtin_available(iOS 12, *)) {
#else
    {
#endif
      unwinder_ = std::make_unique<FramePointerUnwinder>();

      auto test_module =
          std::make_unique<TestModule>(kModuleStart, kModuleSize);
      module_ = test_module.get();
      module_cache_.AddCustomNativeModule(std::move(test_module));
      auto non_native_module = std::make_unique<TestModule>(
          kNonNativeModuleStart, kModuleSize, false);
      non_native_module_ = non_native_module.get();
      std::vector<std::unique_ptr<const ModuleCache::Module>> wrapper;
      wrapper.push_back(std::move(non_native_module));
      module_cache()->UpdateNonNativeModules({}, std::move(wrapper));

      unwinder_->Initialize(&module_cache_);
    }
  }

  ModuleCache* module_cache() { return &module_cache_; }
  ModuleCache::Module* module() { return module_; }
  ModuleCache::Module* non_native_module() { return non_native_module_; }
  Unwinder* unwinder() { return unwinder_.get(); }

 private:
  std::unique_ptr<Unwinder> unwinder_;
  base::ModuleCache module_cache_;
  raw_ptr<ModuleCache::Module> module_;
  raw_ptr<ModuleCache::Module> non_native_module_;
};

TEST_F(FramePointerUnwinderTest, FPPointsOutsideOfStack) {
  InputStack input({
      {false, 0x1000},
      {false, 0x1000},
      {false, 0x1000},
      {false, 0x1000},
      {false, 0x1000},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = 0x1;
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({{kModuleStart, module()}}), stack);

  RegisterContextFramePointer(&context) = input.bottom() - sizeof(uintptr_t);
  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({{kModuleStart, module()}}), stack);

  RegisterContextFramePointer(&context) = input.top();
  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({{kModuleStart, module()}}), stack);
}

TEST_F(FramePointerUnwinderTest, FPPointsToSelf) {
  InputStack input({
      {true, 0},
      {false, kModuleStart + 0x10},
      {true, 4},
      {false, kModuleStart + 0x20},
      {false, 0},
      {false, 0},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = input.bottom();
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({
                {kModuleStart, module()},
            }),
            stack);
}

// Tests that two frame pointers that point to each other can't create an
// infinite loop
TEST_F(FramePointerUnwinderTest, FPCycle) {
  InputStack input({
      {true, 2},
      {false, kModuleStart + 0x10},
      {true, 0},
      {false, kModuleStart + 0x20},
      {true, 4},
      {false, kModuleStart + 0x30},
      {false, 0},
      {false, 0},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = input.bottom();
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({
                {kModuleStart, module()},
                {kModuleStart + 0x10, module()},
            }),
            stack);
}

TEST_F(FramePointerUnwinderTest, NoModuleForIP) {
  uintptr_t not_in_module = kModuleStart - 0x10;
  InputStack input({
      {true, 2},
      {false, not_in_module},
      {true, 4},
      {true, kModuleStart + 0x10},
      {false, 0},
      {false, 0},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = input.bottom();
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(
      std::vector<Frame>({{kModuleStart, module()}, {not_in_module, nullptr}}),
      stack);
}

// Tests that testing that checking if there's space to read two values from the
// stack doesn't overflow.
TEST_F(FramePointerUnwinderTest, FPAdditionOverflows) {
  uintptr_t will_overflow = std::numeric_limits<uintptr_t>::max() - 1;
  InputStack input({
      {true, 2},
      {false, kModuleStart + 0x10},
      {false, 0},
      {false, 0},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = will_overflow;
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kAborted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({
                {kModuleStart, module()},
            }),
            stack);
}

// Tests the happy path: a successful unwind with no non-native modules.
TEST_F(FramePointerUnwinderTest, RegularUnwind) {
  InputStack input({
      {true, 4},                     // fp of frame 1
      {false, kModuleStart + 0x20},  // ip of frame 1
      {false, 0xaaaa},
      {false, 0xaaaa},
      {true, 8},                     // fp of frame 2
      {false, kModuleStart + 0x42},  // ip of frame 2
      {false, 0xaaaa},
      {false, 0xaaaa},
      {false, 0},
      {false, 1},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = input.bottom();
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kCompleted,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({
                {kModuleStart, module()},
                {kModuleStart + 0x20, module()},
                {kModuleStart + 0x42, module()},
            }),
            stack);
}

// Tests that if a V8 frame is encountered, unwinding stops and
// kUnrecognizedFrame is returned to facilitate continuing with the V8 unwinder.
TEST_F(FramePointerUnwinderTest, NonNativeFrame) {
  InputStack input({
      {true, 4},                     // fp of frame 1
      {false, kModuleStart + 0x20},  // ip of frame 1
      {false, 0xaaaa},
      {false, 0xaaaa},
      {true, 8},                              // fp of frame 2
      {false, kNonNativeModuleStart + 0x42},  // ip of frame 2
      {false, 0xaaaa},
      {false, 0xaaaa},
      {true, 12},                    // fp of frame 3
      {false, kModuleStart + 0x10},  // ip of frame 3
      {true, 0xaaaa},
      {true, 0xaaaa},
      {false, 0},
      {false, 1},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = input.bottom();
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kUnrecognizedFrame,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
  EXPECT_EQ(std::vector<Frame>({
                {kModuleStart, module()},
                {kModuleStart + 0x20, module()},
                {kNonNativeModuleStart + 0x42, non_native_module()},
            }),
            stack);
}

// Tests that a V8 frame with an unaligned frame pointer correctly returns
// kUnrecognizedFrame and not kAborted.
TEST_F(FramePointerUnwinderTest, NonNativeUnaligned) {
  InputStack input({
      {true, 4},                     // fp of frame 1
      {false, kModuleStart + 0x20},  // ip of frame 1
      {false, 0xaaaa},
      {false, 0xaaaa},
      {true, 7},                              // fp of frame 2
      {false, kNonNativeModuleStart + 0x42},  // ip of frame 2
      {false, 0xaaaa},
      {true, 10},                    // fp of frame 3
      {false, kModuleStart + 0x10},  // ip of frame 3
      {true, 0xaaaa},
      {false, 0},
      {false, 1},
  });

  RegisterContext context;
  RegisterContextStackPointer(&context) = input.bottom();
  RegisterContextInstructionPointer(&context) = kModuleStart;
  RegisterContextFramePointer(&context) = input.bottom();
  std::vector<Frame> stack = {
      Frame(RegisterContextInstructionPointer(&context), module())};

  EXPECT_EQ(UnwindResult::kUnrecognizedFrame,
            unwinder()->TryUnwind(/*state_capture=*/nullptr, &context,
                                  input.top(), &stack));
}

}  // namespace base
