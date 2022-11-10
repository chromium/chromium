// Copyright 2019 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/profiler/unwinder.h"
#include "base/test/metrics/histogram_tester.h"
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
  raw_ptr<ModuleCache> module_cache_;
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
    std::memcpy(stack_buffer->buffer(), &(*fake_stack_)[0],
                fake_stack_->size() * sizeof((*fake_stack_)[0]));
    *stack_top = reinterpret_cast<uintptr_t>(stack_buffer->buffer() +
                                             fake_stack_->size());
    // Set the stack pointer to be consistent with the copied stack.
    *thread_context = {};
    RegisterContextStackPointer(thread_context) =
        reinterpret_cast<uintptr_t>(stack_buffer->buffer());

    *timestamp = timestamp_;

    return true;
  }

 private:
  // Must be a reference to retain the underlying allocation from the vector
  // passed to the constructor.
  const raw_ref<const std::vector<uintptr_t>> fake_stack_;

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
    *stack_top = reinterpret_cast<uintptr_t>(stack_buffer->buffer()) +
                 10;  // Make msan happy.
    delegate->OnStackCopy();
    return true;
  }
};

// Trivial unwinder implementation for testing.
class TestUnwinder : public Unwinder {
 public:
  explicit TestUnwinder(std::vector<uintptr_t>* stack_copy)
      : stack_copy_(stack_copy) {}

  bool CanUnwindFrom(const Frame& current_frame) const override { return true; }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) override {
    auto* bottom = reinterpret_cast<uintptr_t*>(
        RegisterContextStackPointer(thread_context));
    *stack_copy_ =
        std::vector<uintptr_t>(bottom, reinterpret_cast<uintptr_t*>(stack_top));
    return UnwindResult::kCompleted;
  }

 private:
  raw_ptr<std::vector<uintptr_t>> stack_copy_;
};

// Records invocations of calls to OnStackCapture()/UpdateModules().
class CallRecordingUnwinder : public Unwinder {
 public:
  void OnStackCapture() override { on_stack_capture_was_invoked_ = true; }

  void UpdateModules() override { update_modules_was_invoked_ = true; }

  bool CanUnwindFrom(const Frame& current_frame) const override { return true; }

  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) override {
    return UnwindResult::kUnrecognizedFrame;
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
        : can_unwind(can_unwind), result(UnwindResult::kUnrecognizedFrame) {}

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
                         std::vector<Frame>* stack) override {
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

TEST(StackSamplerTest, CopyStack) {
  ModuleCache module_cache;
  const std::vector<uintptr_t> stack = {0, 1, 2, 3, 4};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  std::unique_ptr<StackSampler> stack_sampler = StackSampler::CreateForTesting(
      std::make_unique<TestStackCopier>(stack),
      MakeUnwindersFactory(std::make_unique<TestUnwinder>(&stack_copy)),
      &module_cache);

  stack_sampler->Initialize();

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);
  stack_sampler->RecordStackFrames(stack_buffer.get(), &profile_builder,
                                   PlatformThread::CurrentId());

  EXPECT_EQ(stack, stack_copy);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(StackSamplerTest, RecordStackFramesUMAMetric) {
  HistogramTester histogram_tester;
  ModuleCache module_cache;
  std::vector<uintptr_t> stack;
  constexpr size_t UIntPtrsPerKilobyte = 1024 / sizeof(uintptr_t);
  // kExpectedSizeKB needs to be a fairly large number of kilobytes. The buckets
  // in UmaHistogramMemoryKB are big enough that small values are in the same
  // bucket as zero and less than zero, and testing that we added a sample in
  // that bucket means that the test won't fail if, for example, the
  // |stack_top - stack_bottom| subtraction was reversed and got a negative
  // value.
  constexpr int kExpectedSizeKB = 2048;
  for (uintptr_t i = 0; i <= (kExpectedSizeKB * UIntPtrsPerKilobyte) + 1; i++) {
    stack.push_back(i);
  }
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  std::unique_ptr<StackSampler> stack_sampler = StackSampler::CreateForTesting(
      std::make_unique<TestStackCopier>(stack),
      MakeUnwindersFactory(std::make_unique<TestUnwinder>(&stack_copy)),
      &module_cache);

  stack_sampler->Initialize();

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);

  for (uint32_t i = 0; i < StackSampler::kUMAHistogramDownsampleAmount - 1;
       i++) {
    stack_sampler->RecordStackFrames(stack_buffer.get(), &profile_builder,
                                     PlatformThread::CurrentId());

    // Should have no new samples in the
    // Memory.StackSamplingProfiler.StackSampleSize histogram.
    histogram_tester.ExpectUniqueSample(
        "Memory.StackSamplingProfiler.StackSampleSize", kExpectedSizeKB, 0);
  }

  stack_sampler->RecordStackFrames(stack_buffer.get(), &profile_builder,
                                   PlatformThread::CurrentId());

  histogram_tester.ExpectUniqueSample(
      "Memory.StackSamplingProfiler.StackSampleSize", kExpectedSizeKB, 1);
}
#endif  // #if BUILDFLAG(IS_CHROMEOS)

TEST(StackSamplerTest, CopyStackTimestamp) {
  ModuleCache module_cache;
  const std::vector<uintptr_t> stack = {0};
  InjectModuleForContextInstructionPointer(stack, &module_cache);
  std::vector<uintptr_t> stack_copy;
  TimeTicks timestamp = TimeTicks::UnixEpoch();
  std::unique_ptr<StackSampler> stack_sampler = StackSampler::CreateForTesting(
      std::make_unique<TestStackCopier>(stack, timestamp),
      MakeUnwindersFactory(std::make_unique<TestUnwinder>(&stack_copy)),
      &module_cache);

  stack_sampler->Initialize();

  std::unique_ptr<StackBuffer> stack_buffer =
      std::make_unique<StackBuffer>(stack.size() * sizeof(uintptr_t));
  TestProfileBuilder profile_builder(&module_cache);
  stack_sampler->RecordStackFrames(stack_buffer.get(), &profile_builder,
                                   PlatformThread::CurrentId());

  EXPECT_EQ(timestamp, profile_builder.last_timestamp());
}

TEST(StackSamplerTest, UnwinderInvokedWhileRecordingStackFrames) {
  std::unique_ptr<StackBuffer> stack_buffer = std::make_unique<StackBuffer>(10);
  auto owned_unwinder = std::make_unique<CallRecordingUnwinder>();
  CallRecordingUnwinder* unwinder = owned_unwinder.get();
  ModuleCache module_cache;
  TestProfileBuilder profile_builder(&module_cache);
  std::unique_ptr<StackSampler> stack_sampler = StackSampler::CreateForTesting(
      std::make_unique<DelegateInvokingStackCopier>(),
      MakeUnwindersFactory(std::move(owned_unwinder)), &module_cache);

  stack_sampler->Initialize();

  stack_sampler->RecordStackFrames(stack_buffer.get(), &profile_builder,
                                   PlatformThread::CurrentId());

  EXPECT_TRUE(unwinder->on_stack_capture_was_invoked());
  EXPECT_TRUE(unwinder->update_modules_was_invoked());
}

TEST(StackSamplerTest, AuxUnwinderInvokedWhileRecordingStackFrames) {
  std::unique_ptr<StackBuffer> stack_buffer = std::make_unique<StackBuffer>(10);
  ModuleCache module_cache;
  TestProfileBuilder profile_builder(&module_cache);
  std::unique_ptr<StackSampler> stack_sampler = StackSampler::CreateForTesting(
      std::make_unique<DelegateInvokingStackCopier>(),
      MakeUnwindersFactory(std::make_unique<CallRecordingUnwinder>()),
      &module_cache);

  stack_sampler->Initialize();

  auto owned_aux_unwinder = std::make_unique<CallRecordingUnwinder>();
  CallRecordingUnwinder* aux_unwinder = owned_aux_unwinder.get();
  stack_sampler->AddAuxUnwinder(std::move(owned_aux_unwinder));

  stack_sampler->RecordStackFrames(stack_buffer.get(), &profile_builder,
                                   PlatformThread::CurrentId());

  EXPECT_TRUE(aux_unwinder->on_stack_capture_was_invoked());
  EXPECT_TRUE(aux_unwinder->update_modules_was_invoked());
}

TEST(StackSamplerTest, WalkStack_Completed) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(1u, 1u));
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::kCompleted, {1u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSampler::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder), nullptr));

  ASSERT_EQ(2u, stack.size());
  EXPECT_EQ(1u, stack[1].instruction_pointer);
}

TEST(StackSamplerTest, WalkStack_Aborted) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();
  module_cache.AddCustomNativeModule(std::make_unique<TestModule>(1u, 1u));
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::kAborted, {1u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSampler::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder), nullptr));

  ASSERT_EQ(2u, stack.size());
  EXPECT_EQ(1u, stack[1].instruction_pointer);
}

TEST(StackSamplerTest, WalkStack_NotUnwound) {
  ModuleCache module_cache;
  RegisterContext thread_context;
  RegisterContextInstructionPointer(&thread_context) =
      GetTestInstructionPointer();
  auto native_unwinder = WrapUnique(
      new FakeTestUnwinder({{UnwindResult::kUnrecognizedFrame, {}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSampler::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder), nullptr));

  ASSERT_EQ(1u, stack.size());
}

TEST(StackSamplerTest, WalkStack_AuxUnwind) {
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
      WrapUnique(new FakeTestUnwinder({{UnwindResult::kAborted, {1u}}}));
  aux_unwinder->Initialize(&module_cache);
  std::vector<Frame> stack = StackSampler::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(nullptr, std::move(aux_unwinder)));

  ASSERT_EQ(2u, stack.size());
  EXPECT_EQ(GetTestInstructionPointer(), stack[0].instruction_pointer);
  EXPECT_EQ(1u, stack[1].instruction_pointer);
}

TEST(StackSamplerTest, WalkStack_AuxThenNative) {
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
      new FakeTestUnwinder({{UnwindResult::kUnrecognizedFrame, {1u}}, false}));
  aux_unwinder->Initialize(&module_cache);
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::kCompleted, {2u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSampler::WalkStackForTesting(
      &module_cache, &thread_context, 0u,
      MakeUnwinderCircularDeque(std::move(native_unwinder),
                                std::move(aux_unwinder)));

  ASSERT_EQ(3u, stack.size());
  EXPECT_EQ(0u, stack[0].instruction_pointer);
  EXPECT_EQ(1u, stack[1].instruction_pointer);
  EXPECT_EQ(2u, stack[2].instruction_pointer);
}

TEST(StackSamplerTest, WalkStack_NativeThenAux) {
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
      {{false}, {UnwindResult::kUnrecognizedFrame, {2u}}, {false}}));
  aux_unwinder->Initialize(&module_cache);
  auto native_unwinder =
      WrapUnique(new FakeTestUnwinder({{UnwindResult::kUnrecognizedFrame, {1u}},
                                       {UnwindResult::kCompleted, {3u}}}));
  native_unwinder->Initialize(&module_cache);

  std::vector<Frame> stack = StackSampler::WalkStackForTesting(
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
