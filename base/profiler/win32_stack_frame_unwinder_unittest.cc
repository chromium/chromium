// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/win32_stack_frame_unwinder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/suspendable_thread_delegate_win.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// The image base returned by LookupFunctionEntry starts at this value and is
// incremented by the same value with each call.
const uintptr_t kImageBaseIncrement = 1 << 20;

class TestUnwindFunctions : public Win32StackFrameUnwinder::UnwindFunctions {
 public:
  TestUnwindFunctions();

  TestUnwindFunctions(const TestUnwindFunctions&) = delete;
  TestUnwindFunctions& operator=(const TestUnwindFunctions&) = delete;

  PRUNTIME_FUNCTION LookupFunctionEntry(DWORD64 program_counter,
                                        PDWORD64 image_base) override;
  void VirtualUnwind(DWORD64 image_base,
                     DWORD64 program_counter,
                     PRUNTIME_FUNCTION runtime_function,
                     CONTEXT* context) override;

  // These functions set whether the next frame will have a RUNTIME_FUNCTION.
  void SetHasRuntimeFunction(CONTEXT* context);
  void SetNoRuntimeFunction(CONTEXT* context);

 private:
  static RUNTIME_FUNCTION* const kInvalidRuntimeFunction;

  DWORD64 expected_program_counter_;
  DWORD64 next_image_base_;
  DWORD64 expected_image_base_;
  raw_ptr<RUNTIME_FUNCTION> next_runtime_function_;
  std::vector<RUNTIME_FUNCTION> runtime_functions_;
};

RUNTIME_FUNCTION* const TestUnwindFunctions::kInvalidRuntimeFunction =
    reinterpret_cast<RUNTIME_FUNCTION*>(static_cast<uintptr_t>(-1));

TestUnwindFunctions::TestUnwindFunctions()
    : expected_program_counter_(0),
      next_image_base_(kImageBaseIncrement),
      expected_image_base_(0),
      next_runtime_function_(kInvalidRuntimeFunction) {}

PRUNTIME_FUNCTION TestUnwindFunctions::LookupFunctionEntry(
    DWORD64 program_counter,
    PDWORD64 image_base) {
  EXPECT_EQ(expected_program_counter_, program_counter);
  *image_base = expected_image_base_ = next_image_base_;
  next_image_base_ += kImageBaseIncrement;
  RUNTIME_FUNCTION* return_value = next_runtime_function_;
  next_runtime_function_ = kInvalidRuntimeFunction;
  return return_value;
}

void TestUnwindFunctions::VirtualUnwind(DWORD64 image_base,
                                        DWORD64 program_counter,
                                        PRUNTIME_FUNCTION runtime_function,
                                        CONTEXT* context) {
  ASSERT_NE(kInvalidRuntimeFunction, runtime_function)
      << "expected call to SetHasRuntimeFunction() or SetNoRuntimeFunction() "
      << "before invoking TryUnwind()";
  EXPECT_EQ(expected_image_base_, image_base);
  expected_image_base_ = 0;
  EXPECT_EQ(expected_program_counter_, program_counter);
  expected_program_counter_ = 0;
  // This function should only be called when LookupFunctionEntry returns
  // a RUNTIME_FUNCTION.
  EXPECT_EQ(&runtime_functions_.back(), runtime_function);
}

static void SetContextPc(CONTEXT* context, DWORD64 val) {
#if defined(ARCH_CPU_ARM64)
  context->Pc = val;
#else
  context->Rip = val;
#endif
}

void TestUnwindFunctions::SetHasRuntimeFunction(CONTEXT* context) {
  RUNTIME_FUNCTION runtime_function = {};
  runtime_function.BeginAddress = 16;
#if defined(ARCH_CPU_ARM64)
  runtime_function.FunctionLength = 256;
#else
  runtime_function.EndAddress = runtime_function.BeginAddress + 256;
#endif

  runtime_functions_.push_back(runtime_function);
  next_runtime_function_ = &runtime_functions_.back();
  expected_program_counter_ =
      next_image_base_ + runtime_function.BeginAddress + 8;
  SetContextPc(context, expected_program_counter_);
}

void TestUnwindFunctions::SetNoRuntimeFunction(CONTEXT* context) {
  expected_program_counter_ = 100;
  SetContextPc(context, expected_program_counter_);
  next_runtime_function_ = nullptr;
}

}  // namespace

class Win32StackFrameUnwinderTest : public testing::Test {
 public:
  Win32StackFrameUnwinderTest(const Win32StackFrameUnwinderTest&) = delete;
  Win32StackFrameUnwinderTest& operator=(const Win32StackFrameUnwinderTest&) =
      delete;

 protected:
  Win32StackFrameUnwinderTest() = default;

  // This exists so that Win32StackFrameUnwinder's constructor can be private
  // with a single friend declaration of this test fixture.
  std::unique_ptr<Win32StackFrameUnwinder> CreateUnwinder();

  // Weak pointer to the unwind functions used by last created unwinder.
  raw_ptr<TestUnwindFunctions, DanglingUntriaged> unwind_functions_;
};

std::unique_ptr<Win32StackFrameUnwinder>
Win32StackFrameUnwinderTest::CreateUnwinder() {
  std::unique_ptr<TestUnwindFunctions> unwind_functions(
      new TestUnwindFunctions);
  unwind_functions_ = unwind_functions.get();
  return WrapUnique(new Win32StackFrameUnwinder(std::move(unwind_functions)));
}

// Checks the case where all frames have unwind information.
TEST_F(Win32StackFrameUnwinderTest, FramesWithUnwindInfo) {
  std::unique_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
  CONTEXT context = {0};

  TestModule stub_module1(kImageBaseIncrement);
  unwind_functions_->SetHasRuntimeFunction(&context);
  EXPECT_TRUE(unwinder->TryUnwind(true, &context, &stub_module1));

  TestModule stub_module2(kImageBaseIncrement * 2);
  unwind_functions_->SetHasRuntimeFunction(&context);
  EXPECT_TRUE(unwinder->TryUnwind(false, &context, &stub_module2));

  TestModule stub_module3(kImageBaseIncrement * 3);
  unwind_functions_->SetHasRuntimeFunction(&context);
  EXPECT_TRUE(unwinder->TryUnwind(false, &context, &stub_module3));
}

// Checks that the CONTEXT's stack pointer gets popped when the top frame has no
// unwind information.
TEST_F(Win32StackFrameUnwinderTest, FrameAtTopWithoutUnwindInfo) {
  std::unique_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
  CONTEXT context = {0};
  DWORD64 next_ip = 0x0123456789abcdef;
  DWORD64 original_rsp = reinterpret_cast<DWORD64>(&next_ip);
#if defined(ARCH_CPU_ARM64)
  context.Sp = original_rsp;
  context.Lr = next_ip;
  context.ContextFlags |= CONTEXT_UNWOUND_TO_CALL;
#else
  context.Rsp = original_rsp;
#endif

  TestModule stub_module(kImageBaseIncrement);
  unwind_functions_->SetNoRuntimeFunction(&context);
  EXPECT_TRUE(unwinder->TryUnwind(true, &context, &stub_module));
#if defined(ARCH_CPU_ARM64)
  EXPECT_EQ(next_ip, context.Pc);
#else
  EXPECT_EQ(next_ip, context.Rip);
  EXPECT_EQ(original_rsp + 8, context.Rsp);
#endif
}

// Checks that a frame below the top of the stack with missing unwind info
// terminates the unwinding.
TEST_F(Win32StackFrameUnwinderTest, FrameBelowTopWithoutUnwindInfo) {
  {
    // First stack, with a bad function below the top of the stack.
    std::unique_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};

    TestModule stub_module(kImageBaseIncrement);
    unwind_functions_->SetNoRuntimeFunction(&context);
    EXPECT_FALSE(unwinder->TryUnwind(false, &context, &stub_module));
  }
}

// Checks that SuspendableThreadDelegateWin::GetRegisters() and SetRegisters()
// round-trip all platform non-volatile registers.
TEST_F(Win32StackFrameUnwinderTest,
       SuspendableThreadDelegateGetAndSetRegisters) {
  SuspendableThreadDelegateWin delegate(
      GetSamplingProfilerCurrentThreadToken());
  CONTEXT context = {0};
#if defined(ARCH_CPU_X86_64)
  context.R12 = 1;
  context.R13 = 2;
  context.R14 = 3;
  context.R15 = 4;
  context.Rdi = 5;
  context.Rsi = 6;
  context.Rbx = 7;
  context.Rbp = 8;
  context.Rsp = 9;
  std::vector<uintptr_t> registers = delegate.GetRegisters(&context);
  ASSERT_EQ(registers.size(), 9u);
  for (size_t i = 0; i < registers.size(); ++i) {
    EXPECT_EQ(registers[i], i + 1);
    registers[i] += 100;
  }
  delegate.SetRegisters(&context, registers);
  EXPECT_EQ(context.R12, 101u);
  EXPECT_EQ(context.R13, 102u);
  EXPECT_EQ(context.R14, 103u);
  EXPECT_EQ(context.R15, 104u);
  EXPECT_EQ(context.Rdi, 105u);
  EXPECT_EQ(context.Rsi, 106u);
  EXPECT_EQ(context.Rbx, 107u);
  EXPECT_EQ(context.Rbp, 108u);
  EXPECT_EQ(context.Rsp, 109u);
#elif defined(ARCH_CPU_ARM64)
  context.X19 = 1;
  context.X20 = 2;
  context.X21 = 3;
  context.X22 = 4;
  context.X23 = 5;
  context.X24 = 6;
  context.X25 = 7;
  context.X26 = 8;
  context.X27 = 9;
  context.X28 = 10;
  context.Fp = 11;
  context.Lr = 12;
  context.Sp = 13;
  std::vector<uintptr_t> registers = delegate.GetRegisters(&context);
  ASSERT_EQ(registers.size(), 13u);
  for (size_t i = 0; i < registers.size(); ++i) {
    EXPECT_EQ(registers[i], i + 1);
    registers[i] += 100;
  }
  delegate.SetRegisters(&context, registers);
  EXPECT_EQ(context.X19, 101u);
  EXPECT_EQ(context.X20, 102u);
  EXPECT_EQ(context.X21, 103u);
  EXPECT_EQ(context.X22, 104u);
  EXPECT_EQ(context.X23, 105u);
  EXPECT_EQ(context.X24, 106u);
  EXPECT_EQ(context.X25, 107u);
  EXPECT_EQ(context.X26, 108u);
  EXPECT_EQ(context.X27, 109u);
  EXPECT_EQ(context.X28, 110u);
  EXPECT_EQ(context.Fp, 111u);
  EXPECT_EQ(context.Lr, 112u);
  EXPECT_EQ(context.Sp, 113u);
#endif
}

}  // namespace base
