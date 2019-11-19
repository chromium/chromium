// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstring>
#include <memory>
#include <numeric>
#include <utility>

#include "base/profiler/profile_builder.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

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
        reinterpret_cast<uintptr_t>(&fake_stack_[0]);
    RegisterContextInstructionPointer(thread_context) =
        reinterpret_cast<uintptr_t>(fake_stack_[0]);
    return true;
  }

  PlatformThreadId GetThreadId() const override { return PlatformThreadId(); }

  uintptr_t GetStackBaseAddress() const override {
    return reinterpret_cast<uintptr_t>(&fake_stack_[0] + fake_stack_.size());
  }

  bool CanCopyStack(uintptr_t stack_pointer) override { return true; }

  std::vector<uintptr_t*> GetRegistersToRewrite(
      RegisterContext* thread_context) override {
    return {&RegisterContextFramePointer(thread_context)};
  }

 private:
  // Must be a reference to retain the underlying allocation from the vector
  // passed to the constructor.
  const std::vector<uintptr_t>& fake_stack_;
  RegisterContext* thread_context_;
};

class TestProfileBuilder : public ProfileBuilder {
 public:
  TestProfileBuilder() = default;

  TestProfileBuilder(const TestProfileBuilder&) = delete;
  TestProfileBuilder& operator=(const TestProfileBuilder&) = delete;

  // ProfileBuilder
  ModuleCache* GetModuleCache() override { return nullptr; }

  void RecordMetadata(
      ProfileBuilder::MetadataProvider* metadata_provider) override {
    recorded_metadata_ = true;
  }

  void OnSampleCompleted(std::vector<Frame> frames) override {}
  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override {}

 private:
  bool recorded_metadata_ = false;
};

}  // namespace

TEST(StackCopierSuspendTest, CopyStack) {
  const std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TestProfileBuilder profile_builder;
  RegisterContext register_context{};
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top,
                                 &profile_builder, &register_context);

  uintptr_t* stack_copy_bottom =
      reinterpret_cast<uintptr_t*>(stack_buffer.get()->buffer());
  std::vector<uintptr_t> stack_copy(stack_copy_bottom,
                                    stack_copy_bottom + stack.size());
  EXPECT_EQ(stack, stack_copy);
}

TEST(StackCopierSuspendTest, CopyStackBufferTooSmall) {
  std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>((stack.size() - 1) * sizeof(uintptr_t));
  // Make the buffer different than the input stack.
  stack_buffer->buffer()[0] = 100;
  uintptr_t stack_top = 0;
  TestProfileBuilder profile_builder;
  RegisterContext register_context{};
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top,
                                 &profile_builder, &register_context);

  uintptr_t* stack_copy_bottom =
      reinterpret_cast<uintptr_t*>(stack_buffer.get()->buffer());
  std::vector<uintptr_t> stack_copy(stack_copy_bottom,
                                    stack_copy_bottom + stack.size());
  // Use the buffer not being overwritten as a proxy for the unwind being
  // aborted.
  EXPECT_NE(stack, stack_copy);
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
  TestProfileBuilder profile_builder;
  RegisterContext register_context{};
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top,
                                 &profile_builder, &register_context);

  uintptr_t* stack_copy_bottom =
      reinterpret_cast<uintptr_t*>(stack_buffer.get()->buffer());
  std::vector<uintptr_t> stack_copy(stack_copy_bottom,
                                    stack_copy_bottom + stack.size());
  EXPECT_THAT(stack_copy,
              ElementsAre(reinterpret_cast<uintptr_t>(stack_copy_bottom),
                          reinterpret_cast<uintptr_t>(stack_copy_bottom) +
                              sizeof(uintptr_t)));
}

TEST(StackCopierSuspendTest, RewriteRegisters) {
  std::vector<uintptr_t> stack = {0, 1, 2};
  RegisterContext register_context{};
  RegisterContextFramePointer(&register_context) =
      reinterpret_cast<uintptr_t>(&stack[1]);
  StackCopierSuspend stack_copier_suspend(
      std::make_unique<TestSuspendableThreadDelegate>(stack,
                                                      &register_context));

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  uintptr_t stack_top = 0;
  TestProfileBuilder profile_builder;
  stack_copier_suspend.CopyStack(stack_buffer.get(), &stack_top,
                                 &profile_builder, &register_context);

  uintptr_t stack_copy_bottom =
      reinterpret_cast<uintptr_t>(stack_buffer.get()->buffer());
  EXPECT_EQ(stack_copy_bottom + sizeof(uintptr_t),
            RegisterContextFramePointer(&register_context));
}

}  // namespace base
