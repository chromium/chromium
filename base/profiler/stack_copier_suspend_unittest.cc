// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <algorithm>
#include <cstring>
#include <memory>
#include <numeric>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/memory/page_size.h"
#endif

namespace base {

namespace {

using ::testing::Each;
using ::testing::ElementsAre;

// A thread delegate for use in tests that provides the expected behavior when
// operating on the supplied fake stack.
class TestSuspendableThreadDelegate : public SuspendableThreadDelegate {
 public:
  class TestScopedSuspendThread
      : public SuspendableThreadDelegate::ScopedSuspendThread {
   public:
    TestScopedSuspendThread() = default;

    TestScopedSuspendThread(const TestScopedSuspendThread&) = delete;
    TestScopedSuspendThread& operator=(const TestScopedSuspendThread&) = delete;

    bool WasSuccessful() const override { return true; }
  };

  TestSuspendableThreadDelegate(const std::vector<uintptr_t>& fake_stack,
                                // The register context will be initialized to
                                // *|thread_context| if non-null.
                                RegisterContext* thread_context = nullptr)
      : fake_stack_(fake_stack), thread_context_(thread_context) {}

  TestSuspendableThreadDelegate(const TestSuspendableThreadDelegate&) = delete;
  TestSuspendableThreadDelegate& operator=(
      const TestSuspendableThreadDelegate&) = delete;

  std::unique_ptr<ScopedSuspendThread> CreateScopedSuspendThread() override {
    return std::make_unique<TestScopedSuspendThread>();
  }

  bool GetThreadContext(RegisterContext* thread_context) override {
    if (thread_context_)
      *thread_context = *thread_context_;
    // Set the stack pointer to be consistent with the provided fake stack.
    RegisterContextStackPointer(thread_context) =
        reinterpret_cast<uintptr_t>(&(*fake_stack_)[0]);
    RegisterContextInstructionPointer(thread_context) =
        reinterpret_cast<uintptr_t>((*fake_stack_)[0]);
    return true;
  }

  PlatformThreadId GetThreadId() const override { return PlatformThreadId(); }

  uintptr_t GetStackBaseAddress() const override {
    return reinterpret_cast<uintptr_t>(&(*fake_stack_)[0] +
                                       fake_stack_->size());
  }

  bool CanCopyStack(uintptr_t stack_pointer) override { return true; }

  std::vector<uintptr_t*> GetRegistersToRewrite(
      RegisterContext* thread_context) override {
    return {&RegisterContextFramePointer(thread_context)};
  }

 private:
  // Must be a reference to retain the underlying allocation from the vector
  // passed to the constructor.
  const raw_ref<const std::vector<uintptr_t>> fake_stack_;
  raw_ptr<RegisterContext> thread_context_;
};

class TestStackCopierDelegate : public StackCopier::Delegate {
 public:
  void OnStackCopy() override {
    on_stack_copy_was_invoked_ = true;
  }

  bool on_stack_copy_was_invoked() const { return on_stack_copy_was_invoked_; }

 private:
  bool on_stack_copy_was_invoked_ = false;
};

}  // namespace

TEST(StackCopierSuspendTest, CopyStack) {
  const std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext register_context{};
  TestStackCopierDelegate stack_copier_delegate;
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top, &timestamp,
                                 &register_context, &stack_copier_delegate);

  uintptr_t* stack_copy_bottom =
      reinterpret_cast<uintptr_t*>(stack_buffer.get()->buffer());
  std::vector<uintptr_t> stack_copy(stack_copy_bottom,
                                    stack_copy_bottom + stack.size());
  EXPECT_EQ(stack, stack_copy);
}

TEST(StackCopierSuspendTest, CopyStackBufferTooSmall) {
  std::vector<uintptr_t> stack;
#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS will round up the size of the stack up to the next multiple of
  // the page size. To make the buffer "too small", the stack must be 1 element
  // larger than the page size.
  const size_t kStackElements = (GetPageSize() / sizeof(stack[0])) + 1;
#else  // #if BUILDFLAG(IS_CHROMEOS)
  const size_t kStackElements = 5;  // Arbitrary
#endif
  stack.reserve(kStackElements);
  for (size_t i = 0; i < kStackElements; ++i) {
    stack.push_back(i);
  }
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>((stack.size() - 1) * sizeof(stack[0]));
  // Make the buffer different than the input stack.
  constexpr uintptr_t kBufferInitializer = 100;
  size_t stack_buffer_elements =
      stack_buffer->size() / sizeof(stack_buffer->buffer()[0]);
  std::fill_n(stack_buffer->buffer(), stack_buffer_elements,
              kBufferInitializer);
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext register_context{};
  TestStackCopierDelegate stack_copier_delegate;
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top, &timestamp,
                                 &register_context, &stack_copier_delegate);

  uintptr_t* stack_copy_bottom =
      reinterpret_cast<uintptr_t*>(stack_buffer.get()->buffer());
  std::vector<uintptr_t> stack_copy(stack_copy_bottom,
                                    stack_copy_bottom + stack_buffer_elements);
  // Use the buffer not being overwritten as a proxy for the unwind being
  // aborted.
  EXPECT_THAT(stack_copy, Each(kBufferInitializer));
}

TEST(StackCopierSuspendTest, CopyStackAndRewritePointers) {
  // Allocate space for the stack, then make its elements point to themselves.
  std::vector<uintptr_t> stack(2);
  stack[0] = reinterpret_cast<uintptr_t>(&stack[0]);
  stack[1] = reinterpret_cast<uintptr_t>(&stack[1]);
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext register_context{};
  TestStackCopierDelegate stack_copier_delegate;
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top, &timestamp,
                                 &register_context, &stack_copier_delegate);

  uintptr_t* stack_copy_bottom =
      reinterpret_cast<uintptr_t*>(stack_buffer.get()->buffer());
  std::vector<uintptr_t> stack_copy(stack_copy_bottom,
                                    stack_copy_bottom + stack.size());
  EXPECT_THAT(stack_copy,
              ElementsAre(reinterpret_cast<uintptr_t>(stack_copy_bottom),
                          reinterpret_cast<uintptr_t>(stack_copy_bottom) +
                              sizeof(uintptr_t)));
}

TEST(StackCopierSuspendTest, CopyStackTimeStamp) {
  const std::vector<uintptr_t> stack = {0};
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext register_context{};
  TestStackCopierDelegate stack_copier_delegate;

  TimeTicks before = TimeTicks::Now();
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top, &timestamp,
                                 &register_context, &stack_copier_delegate);
  TimeTicks after = TimeTicks::Now();

  EXPECT_GE(timestamp, before);
  EXPECT_LE(timestamp, after);
}

TEST(StackCopierSuspendTest, CopyStackDelegateInvoked) {
  const std::vector<uintptr_t> stack = {0};
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  RegisterContext register_context{};
  TestStackCopierDelegate stack_copier_delegate;

  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top, &timestamp,
                                 &register_context, &stack_copier_delegate);

  EXPECT_TRUE(stack_copier_delegate.on_stack_copy_was_invoked());
}

TEST(StackCopierSuspendTest, RewriteRegisters) {
  std::vector<uintptr_t> stack = {0, 1, 2};
  RegisterContext register_context{};
  TestStackCopierDelegate stack_copier_delegate;
  RegisterContextFramePointer(&register_context) =
      reinterpret_cast<uintptr_t>(&stack[1]);
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack,
                                                      &register_context));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TimeTicks timestamp;
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top, &timestamp,
                                 &register_context, &stack_copier_delegate);

  uintptr_t stack_copy_bottom =
      reinterpret_cast<uintptr_t>(stack_buffer.get()->buffer());
  EXPECT_EQ(stack_copy_bottom + sizeof(uintptr_t),
            RegisterContextFramePointer(&register_context));
}

}  // namespace base
