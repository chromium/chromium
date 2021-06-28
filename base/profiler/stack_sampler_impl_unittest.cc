// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstring>
#include <iterator>
#include <memory>
#include <numeric>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/stack_sampler_impl.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/profiler/unwinder.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::ElementsAre;

class TestProfileBuilder : public ProfileBuilder {
 public:
  TestProfileBuilder(ModuleCache* module_cache) : module_cache_(module_cache) {}

  TestProfileBuilder(const TestProfileBuilder&) = delete;
  TestProfileBuilder& operator=(const TestProfileBuilder&) = delete;

  // ProfileBuilder
  ModuleCache* GetModuleCache() override { return module_cache_; }
  void RecordMetadata(
      const MetadataRecorder::MetadataProvider& metadata_provider) override {}

  void OnSampleCompleted(std::vector<Frame> frames,
                         TimeTicks sample_timestamp) override {
    last_timestamp_ = sample_timestamp;
  }

  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override {}

  TimeTicks last_timestamp() { return last_timestamp_; }

 private:
  ModuleCache* module_cache_;
  TimeTicks last_timestamp_;
};

// A stack copier for use in tests that provides the expected behavior when
// operating on the supplied fake stack.
class TestStackCopier : public StackCopier {
 public:
  TestStackCopier(const std::vector<uintptr_t>& fake_stack,
                  TimeTicks timestamp = TimeTicks())
      : fake_stack_(fake_stack), timestamp_(timestamp) {}

  bool CopyStack(StackBuffer* stack_buffer,
                 uintptr_t* stack_top,
                 TimeTicks* timestamp,
                 RegisterContext* thread_context,
                 Delegate* delegate) override {
    std::memcpy(stack_buffer->buffer(), &fake_stack_[0], fake_stack_.size());
    *stack_top =
        reinterpret_cast<uintptr_t>(&fake_stack_[0] + fake_stack_.size());
    // Set the stack pointer to be consistent with the provided fake stack.
    *thread_context = {};
    RegisterContextStackPointer(thread_context) =
        reinterpret_cast<uintptr_t>(&fake_stack_[0]);

    *timestamp = timestamp_;

    return true;
  }

 private:
  // Must be a reference to retain the underlying allocation from the vector
  // passed to the constructor.
  const std::vector<uintptr_t>& fake_stack_;

  const TimeTicks timestamp_;
};

// A StackCopier that just invokes the expected functions on the delegate.
class DelegateInvokingStackCopier : public StackCopier {
 public:
  bool CopyStack(StackBuffer* stack_buffer,
                 uintptr_t* stack_top,
                 TimeTicks* timestamp,
                 RegisterContext* thread_context,
                 Delegate* delegate) override {
    delegate->OnStackCopy();
    return true;
  }
};

// Trivial unwinder implementation for testing.
class TestUnwinder : public Unwinder {
 public:
  TestUnwinder(size_t stack_size = 0,
               std::vector<uintptr_t>* stack_copy = nullptr,
               // Variable to fill in with the bottom address of the
               // copied stack. This will be different than
               // &(*stack_copy)[0] because |stack_copy| is a copy of the
               // copy so does not share memory with the actual copy.
               uintptr_t* stack_copy_bottom = nullptr)
      : stack_size_(stack_size),
        stack_copy_(stack_copy),
        stack_copy_bottom_(stack_copy_bottom) {}

  bool CanUnwindFrom(const Frame& current_frame) const override { return true; }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) const override {
    if (stack_copy_) {
      auto* bottom = reinterpret_cast<uintptr_t*>(
          RegisterContextStackPointer(thread_context));
      auto* top = bottom + stack_size_;
      *stack_copy_ = std::vector<uintptr_t>(bottom, top);
    }
    if (stack_copy_bottom_)
      *stack_copy_bottom_ = RegisterContextStackPointer(thread_context);
    return UnwindResult::COMPLETED;
  }

 private:
  size_t stack_size_;
  std::vector<uintptr_t>* stack_copy_;
  uintptr_t* stack_copy_bottom_;
};

// Records invocations of calls to OnStackCapture()/UpdateModules().
class CallRecordingUnwinder : public Unwinder {
 public:
  void OnStackCapture() override { on_stack_capture_was_invoked_ = true; }

  void UpdateModules() override { update_modules_was_invoked_ = true; }

  bool CanUnwindFrom(const Frame& current_frame) const override { return true; }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) const override {
    return UnwindResult::UNRECOGNIZED_FRAME;
  }

  bool on_stack_capture_was_invoked() const {
    return on_stack_capture_was_invoked_;
  }

  bool update_modules_was_invoked() const {
    return update_modules_was_invoked_;
  }

 private:
  bool on_stack_capture_was_invoked_ = false;
  bool update_modules_was_invoked_ = false;
};

class TestModule : public ModuleCache::Module {
 public:
  TestModule(uintptr_t base_address, size_t size, bool is_native = true)
      : base_address_(base_address), size_(size), is_native_(is_native) {}

  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return ""; }
  FilePath GetDebugBasename() const override { return FilePath(); }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return is_native_; }

 private:
  const uintptr_t base_address_;
  const size_t size_;
  const bool is_native_;
};

// Utility function to form a vector from a single module.
std::vector<std::unique_ptr<const ModuleCache::Module>> ToModuleVector(
    std::unique_ptr<const ModuleCache::Module> module) {
  return std::vector<std::unique_ptr<const ModuleCache::Module>>(
      std::make_move_iterator(&module), std::make_move_iterator(&module + 1));
}

// Injects a fake module covering the initial instruction pointer value, to
// avoid asking the OS to look it up. Windows doesn't return a consistent error
// code when doing so, and we DCHECK_EQ the expected error code.
void InjectModuleForContextInstructionPointer(
    const std::vector<uintptr_t>& stack,
    ModuleCache* module_cache) {
  module_cache->AddCustomNativeModule(
      std::make_unique<TestModule>(stack[0], sizeof(uintptr_t)));
}

// Returns a plausible instruction pointer value for use in tests that don't
// care about the instruction pointer value in the context, and hence don't need
// InjectModuleForContextInstructionPointer().
uintptr_t GetTestInstructionPointer() {
  return reinterpret_cast<uintptr_t>(&GetTestInstructionPointer);
}

// An unwinder fake that replays the provided outputs.
class FakeTestUnwinder : public Unwinder {
 public:
  struct Result {
    Result(bool can_unwind)
        : can_unwind(can_unwind), result(UnwindResult::UNRECOGNIZED_FRAME) {}

    Result(UnwindResult result, std::vector<uintptr_t> instruction_pointers)
        : can_unwind(true),
          result(result),
          instruction_pointers(instruction_pointers) {}

    bool can_unwind;
    UnwindResult result;
    std::vector<uintptr_t> instruction_pointers;
  };

  // Construct the unwinder with the outputs. The relevant unwinder functions
  // are expected to be invoked at least as many times as the number of values
  // specified in the arrays (except for CanUnwindFrom() which will always
  // return true if provided an empty array.
  explicit FakeTestUnwinder(std::vector<Result> results)
      : results_(std::move(results)) {}

  FakeTestUnwinder(const FakeTestUnwinder&) = delete;
  FakeTestUnwinder& operator=(const FakeTestUnwinder&) = delete;

  bool CanUnwindFrom(const Frame& current_frame) const override {
    bool can_unwind = results_[current_unwind_].can_unwind;
    // NB: If CanUnwindFrom() returns false then TryUnwind() will not be
    // invoked, so current_unwind_ is guarantee to be incremented only once for
    // each result.
    if (!can_unwind)
      ++current_unwind_;
    return can_unwind;
  }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) const override {
    CHECK_LT(current_unwind_, results_.size());
    const Result& current_result = results_[current_unwind_];
    ++current_unwind_;
    CHECK(current_result.can_unwind);
    for (const auto instruction_pointer : current_result.instruction_pointers)
      stack->emplace_back(
          instruction_pointer,
          module_cache()->GetModuleForAddress(instruction_pointer));
    return current_result.result;
  }

 private:
  mutable size_t current_unwind_ = 0;
  std::vector<Result> results_;
};

StackSampler::UnwindersFactory MakeUnwindersFactory(
    std::unique_ptr<Unwinder> unwinder) {
  return BindOnce(
      [](std::unique_ptr<Unwinder> unwinder) {
        std::vector<std::unique_ptr<Unwinder>> unwinders;
        unwinders.push_back(std::move(unwinder));
        return unwinders;
      },
      std::move(unwinder));
}

base::circular_deque<std::unique_ptr<Unwinder>> MakeUnwinderCircularDeque(
    std::unique_ptr<Unwinder> native_unwinder,
    std::unique_ptr<Unwinder> aux_unwinder) {
  base::circular_deque<std::unique_ptr<Unwinder>> unwinders;
  if (native_unwinder)
    unwinders.push_front(std::move(native_unwinder));
  if (aux_unwinder)
    unwinders.push_front(std::move(aux_unwinder));
  return unwinders;
}

}  // namespace

// TODO(crbug.com/1001923): Fails on Linux MSan.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_CopyStack DISABLED_MAYBE_CopyStack
#else
#define MAYBE_CopyStack CopyStack
#endif
TEST(StackSamplerImplTest, MAYBE_CopyStack) {
  ModuleCache module_cache;
  const std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestStackCopier>(stack),
      MakeUnwindersFactory(
          std::make_unique<TestUnwinder>(stack.size(), &stack_copy)),
      &module_cache);

  stack_sampler_impl.Initialize();

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);
  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_EQ(stack, stack_copy);
}

TEST(StackSamplerImplTest, CopyStackTimestamp) {
  ModuleCache module_cache;
  const std::vector<uintptr_t> stack = {0};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  TimeTicks timestamp = TimeTicks::UnixEpoch();
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<TestStackCopier>(stack, timestamp),
      MakeUnwindersFactory(
          std::make_unique<TestUnwinder>(stack.size(), &stack_copy)),
      &module_cache);

  stack_sampler_impl.Initialize();

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);
  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_EQ(timestamp, profile_builder.last_timestamp());
}

TEST(StackSamplerImplTest, UnwinderInvokedWhileRecordingStackFrames) {
  std::unique_ptr<StackBuffer> stack_buffer = std::make_unique<StackBuffer>(10);
  auto owned_unwinder = std::make_unique<CallRecordingUnwinder>();
  CallRecordingUnwinder* unwinder = owned_unwinder.get();
  ModuleCache module_cache;
  TestProfileBuilder profile_builder(&module_cache);
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<DelegateInvokingStackCopier>(),
      MakeUnwindersFactory(std::move(owned_unwinder)), &module_cache);

  stack_sampler_impl.Initialize();

  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_TRUE(unwinder->on_stack_capture_was_invoked());
  EXPECT_TRUE(unwinder->update_modules_was_invoked());
}

TEST(StackSamplerImplTest, AuxUnwinderInvokedWhileRecordingStackFrames) {
  std::unique_ptr<StackBuffer> stack_buffer = std::make_unique<StackBuffer>(10);
  ModuleCache module_cache;
  TestProfileBuilder profile_builder(&module_cache);
  StackSamplerImpl stack_sampler_impl(
      std::make_unique<DelegateInvokingStackCopier>(),
      MakeUnwindersFactory(std::make_unique<CallRecordingUnwinder>()),
      &module_cache);

  stack_sampler_impl.Initialize();

  auto owned_aux_unwinder = std::make_unique<CallRecordingUnwinder>();
  CallRecordingUnwinder* aux_unwinder = owned_aux_unwinder.get();
  stack_sampler_impl.AddAuxUnwinder(std::move(owned_aux_unwinder));

  stack_sampler_impl.RecordStackFrames(stack_buffer.get(), &profile_builder);

  EXPECT_TRUE(aux_unwinder->on_stack_capture_was_invoked());
  EXPECT_TRUE(aux_unwinder->update_modules_was_invoked());
}

TEST(StackSamplerImplTest, WalkStack_Completed) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(1u, 1u));
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::COMPLETED, {1u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSamplerImpl::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder), nullptr));

  ASSERT_EQ(2u, stack.size());
  EXPECT_EQ(1u, stack[1].instruction_pointer);
}

TEST(StackSamplerImplTest, WalkStack_Aborted) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(1u, 1u));
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::ABORTED, {1u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSamplerImpl::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder), nullptr));

  ASSERT_EQ(2u, stack.size());
  EXPECT_EQ(1u, stack[1].instruction_pointer);
}

TEST(StackSamplerImplTest, WalkStack_NotUnwound) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();
  auto native_unwinder = WrapUnique(
      new FakeTestUnwinder({{UnwindResult::UNRECOGNIZED_FRAME, {}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSamplerImpl::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder), nullptr));

  ASSERT_EQ(1u, stack.size());
}

TEST(StackSamplerImplTest, WalkStack_AuxUnwind) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();

  // Treat the context instruction pointer as being in the aux unwinder's
  // non-native module.
  module_cache.UpdateNonNativeModules(
      {}, ToModuleVector(std::make_unique<TestModule>(
              GetTestInstructionPointer(), 1u, false)));

  auto aux_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::ABORTED, {1u}}}));
  aux_unwinder->Initialize(&module_cache);
  std::vector<Frame> stack = StackSamplerImpl::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(nullptr, std::move(aux_unwinder)));

  ASSERT_EQ(2u, stack.size());
  EXPECT_EQ(GetTestInstructionPointer(), stack[0].instruction_pointer);
  EXPECT_EQ(1u, stack[1].instruction_pointer);
}

TEST(StackSamplerImplTest, WalkStack_AuxThenNative) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) = 0u;

  // Treat the context instruction pointer as being in the aux unwinder's
  // non-native module.
  module_cache.UpdateNonNativeModules(
      {}, ToModuleVector(std::make_unique<TestModule>(0u, 1u, false)));
  // Inject a fake native module for the second frame.
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(1u, 1u));

  auto aux_unwinder = WrapUnique(
      new FakeTestUnwinder({{UnwindResult::UNRECOGNIZED_FRAME, {1u}}, false}));
  aux_unwinder->Initialize(&module_cache);
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::COMPLETED, {2u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSamplerImpl::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder),
                                std::move(aux_unwinder)));

  ASSERT_EQ(3u, stack.size());
  EXPECT_EQ(0u, stack[0].instruction_pointer);
  EXPECT_EQ(1u, stack[1].instruction_pointer);
  EXPECT_EQ(2u, stack[2].instruction_pointer);
}

TEST(StackSamplerImplTest, WalkStack_NativeThenAux) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) = 0u;

  // Inject fake native modules for the instruction pointer from the context and
  // the third frame.
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(0u, 1u));
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(2u, 1u));
  // Treat the second frame's pointer as being in the aux unwinder's non-native
  // module.
  module_cache.UpdateNonNativeModules(
      {}, ToModuleVector(std::make_unique<TestModule>(1u, 1u, false)));

  auto aux_unwinder = WrapUnique(new FakeTestUnwinder(
      {{false}, {UnwindResult::UNRECOGNIZED_FRAME, {2u}}, {false}}));
  aux_unwinder->Initialize(&module_cache);
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::UNRECOGNIZED_FRAME, {1u}},
                                       {UnwindResult::COMPLETED, {3u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSamplerImpl::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder),
                                std::move(aux_unwinder)));

  ASSERT_EQ(4u, stack.size());
  EXPECT_EQ(0u, stack[0].instruction_pointer);
  EXPECT_EQ(1u, stack[1].instruction_pointer);
  EXPECT_EQ(2u, stack[2].instruction_pointer);
  EXPECT_EQ(3u, stack[3].instruction_pointer);
}

}  // namespace base
