// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampling_profiler.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notimplemented.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/stack_sampler.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/profiler/unwinder.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <intrin.h>
#include <malloc.h>
#else
#include <alloca.h>
#endif

// STACK_SAMPLING_PROFILER_SUPPORTED is used to conditionally enable the tests
// below for supported platforms (currently Win x64, Mac, iOS 64, some
// Android, and ChromeOS x64).
// ChromeOS: These don't run under MSan because parts of the stack aren't
// initialized.
#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_X86_64)) || (BUILDFLAG(IS_MAC)) || \
    (BUILDFLAG(IS_IOS) && defined(ARCH_CPU_64_BITS)) ||                       \
    (BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)) ||             \
    (BUILDFLAG(IS_CHROMEOS) &&                                                \
     (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64)) &&                 \
     !defined(MEMORY_SANITIZER))
#define STACK_SAMPLING_PROFILER_SUPPORTED 1
#endif

namespace base {

#if defined(STACK_SAMPLING_PROFILER_SUPPORTED)
#define PROFILER_TEST_F(TestClass, TestName) TEST_F(TestClass, TestName)
#else
#define PROFILER_TEST_F(TestClass, TestName) \
  TEST_F(TestClass, DISABLED_##TestName)
#endif

using SamplingParams = StackSamplingProfiler::SamplingParams;

namespace {

// State provided to the ProfileBuilder's ApplyMetadataRetrospectively function.
struct RetrospectiveMetadata {
  TimeTicks period_start;
  TimeTicks period_end;
  MetadataRecorder::Item item;
};

// Profile consists of a set of samples and other sampling information.
struct Profile {
  // The collected samples.
  std::vector<std::vector<Frame>> samples;

  // The number of invocations of RecordMetadata().
  int record_metadata_count;

  // The retrospective metadata requests.
  std::vector<RetrospectiveMetadata> retrospective_metadata;

  // The profile metadata requests.
  std::vector<MetadataRecorder::Item> profile_metadata;

  // Duration of this profile.
  TimeDelta profile_duration;

  // Time between samples.
  TimeDelta sampling_period;
};

// The callback type used to collect a profile. The passed Profile is move-only.
// Other threads, including the UI thread, may block on callback completion so
// this should run as quickly as possible.
using ProfileCompletedCallback = OnceCallback<void(Profile)>;

// TestProfileBuilder collects samples produced by the profiler.
class TestProfileBuilder : public ProfileBuilder {
 public:
  TestProfileBuilder(ModuleCache* module_cache,
                     ProfileCompletedCallback callback);

  TestProfileBuilder(const TestProfileBuilder&) = delete;
  TestProfileBuilder& operator=(const TestProfileBuilder&) = delete;

  ~TestProfileBuilder() override;

  // ProfileBuilder:
  ModuleCache* GetModuleCache() override;
  void RecordMetadata(
      const MetadataRecorder::MetadataProvider& metadata_provider) override;
  void ApplyMetadataRetrospectively(
      TimeTicks period_start,
      TimeTicks period_end,
      const MetadataRecorder::Item& item) override;
  void AddProfileMetadata(const MetadataRecorder::Item& item) override;
  void OnSampleCompleted(std::vector<Frame> sample,
                         TimeTicks sample_timestamp) override;
  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override;

 private:
  raw_ptr<ModuleCache> module_cache_;

  // The set of recorded samples.
  std::vector<std::vector<Frame>> samples_;

  // The number of invocations of RecordMetadata().
  int record_metadata_count_ = 0;

  // The retrospective metadata requests.
  std::vector<RetrospectiveMetadata> retrospective_metadata_;

  // The profile metadata requests.
  std::vector<MetadataRecorder::Item> profile_metadata_;

  // Callback made when sampling a profile completes.
  ProfileCompletedCallback callback_;
};

TestProfileBuilder::TestProfileBuilder(ModuleCache* module_cache,
                                       ProfileCompletedCallback callback)
    : module_cache_(module_cache), callback_(std::move(callback)) {}

TestProfileBuilder::~TestProfileBuilder() = default;

ModuleCache* TestProfileBuilder::GetModuleCache() {
  return module_cache_;
}

void TestProfileBuilder::RecordMetadata(
    const MetadataRecorder::MetadataProvider& metadata_provider) {
  ++record_metadata_count_;
}

void TestProfileBuilder::ApplyMetadataRetrospectively(
    TimeTicks period_start,
    TimeTicks period_end,
    const MetadataRecorder::Item& item) {
  retrospective_metadata_.push_back(
      RetrospectiveMetadata{period_start, period_end, item});
}

void TestProfileBuilder::AddProfileMetadata(
    const MetadataRecorder::Item& item) {
  profile_metadata_.push_back(item);
}

void TestProfileBuilder::OnSampleCompleted(std::vector<Frame> sample,
                                           TimeTicks sample_timestamp) {
  samples_.push_back(std::move(sample));
}

void TestProfileBuilder::OnProfileCompleted(TimeDelta profile_duration,
                                            TimeDelta sampling_period) {
  std::move(callback_).Run(Profile{samples_, record_metadata_count_,
                                   retrospective_metadata_, profile_metadata_,
                                   profile_duration, sampling_period});
}

// Unloads |library| and returns when it has completed unloading. Unloading a
// library is asynchronous on Windows, so simply calling UnloadNativeLibrary()
// is insufficient to ensure it's been unloaded.
void SynchronousUnloadNativeLibrary(NativeLibrary library) {
  UnloadNativeLibrary(library);
#if BUILDFLAG(IS_WIN)
  // NativeLibrary is a typedef for HMODULE, which is actually the base address
  // of the module.
  uintptr_t module_base_address = reinterpret_cast<uintptr_t>(library);
  HMODULE module_handle;
  // Keep trying to get the module handle until the call fails.
  while (::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCTSTR>(module_base_address),
                             &module_handle) ||
         ::GetLastError() != ERROR_MOD_NOT_FOUND) {
    PlatformThread::Sleep(Milliseconds(1));
  }
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// Unloading a library on Mac and Android is synchronous.
#else
  NOTIMPLEMENTED();
#endif
}

void WithTargetThread(ProfileCallback profile_callback) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));
  WithTargetThread(&scenario, std::move(profile_callback));
}

struct TestProfilerInfo {
  TestProfilerInfo(SamplingProfilerThreadToken thread_token,
                   const SamplingParams& params,
                   ModuleCache* module_cache,
                   StackSamplerTestDelegate* delegate = nullptr)
      : completed(WaitableEvent::ResetPolicy::MANUAL,
                  WaitableEvent::InitialState::NOT_SIGNALED),
        profiler(thread_token,
                 params,
                 std::make_unique<TestProfileBuilder>(
                     module_cache,
                     BindLambdaForTesting([this](Profile result_profile) {
                       profile = std::move(result_profile);
                       completed.Signal();
                     })),
                 CreateCoreUnwindersFactoryForTesting(module_cache),
                 RepeatingClosure(),
                 delegate) {}

  TestProfilerInfo(const TestProfilerInfo&) = delete;
  TestProfilerInfo& operator=(const TestProfilerInfo&) = delete;

  // The order here is important to ensure objects being referenced don't get
  // destructed until after the objects referencing them.
  Profile profile;
  WaitableEvent completed;
  StackSamplingProfiler profiler;
};

// Captures samples as specified by |params| on the TargetThread, and returns
// them. Waits up to |profiler_wait_time| for the profiler to complete.
std::vector<std::vector<Frame>> CaptureSamples(const SamplingParams& params,
                                               TimeDelta profiler_wait_time,
                                               ModuleCache* module_cache) {
  std::vector<std::vector<Frame>> samples;
  WithTargetThread(BindLambdaForTesting(
      [&](SamplingProfilerThreadToken target_thread_token) {
        TestProfilerInfo info(target_thread_token, params, module_cache);
        info.profiler.Start();
        info.completed.TimedWait(profiler_wait_time);
        info.profiler.Stop();
        info.completed.Wait();
        samples = std::move(info.profile.samples);
      }));

  return samples;
}

// Waits for one of multiple samplings to complete.
size_t WaitForSamplingComplete(
    const std::vector<std::unique_ptr<TestProfilerInfo>>& infos) {
  // Map unique_ptrs to something that WaitMany can accept.
  std::vector<WaitableEvent*> sampling_completed_rawptrs(infos.size());
  ranges::transform(infos, sampling_completed_rawptrs.begin(),
                    [](const std::unique_ptr<TestProfilerInfo>& info) {
                      return &info.get()->completed;
                    });
  // Wait for one profiler to finish.
  return WaitableEvent::WaitMany(sampling_completed_rawptrs.data(),
                                 sampling_completed_rawptrs.size());
}

// Returns a duration that is longer than the test timeout. We would use
// TimeDelta::Max() but https://crbug.com/465948.
TimeDelta AVeryLongTimeDelta() {
  return Days(1);
}

// Tests the scenario where the library is unloaded after copying the stack, but
// before walking it. If |wait_until_unloaded| is true, ensures that the
// asynchronous library loading has completed before walking the stack. If
// false, the unloading may still be occurring during the stack walk.
void TestLibraryUnload(bool wait_until_unloaded, ModuleCache* module_cache) {
  // Test delegate that supports intervening between the copying of the stack
  // and the walking of the stack.
  class StackCopiedSignaler : public StackSamplerTestDelegate {
   public:
    StackCopiedSignaler(WaitableEvent* stack_copied,
                        WaitableEvent* start_stack_walk,
                        bool wait_to_walk_stack)
        : stack_copied_(stack_copied),
          start_stack_walk_(start_stack_walk),
          wait_to_walk_stack_(wait_to_walk_stack) {}

    void OnPreStackWalk() override {
      stack_copied_->Signal();
      if (wait_to_walk_stack_)
        start_stack_walk_->Wait();
    }

   private:
    const raw_ptr<WaitableEvent> stack_copied_;
    const raw_ptr<WaitableEvent> start_stack_walk_;
    const bool wait_to_walk_stack_;
  };

  SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  NativeLibrary other_library = LoadOtherLibrary();

  // TODO(crbug.com/40061562): Remove `UnsafeDanglingUntriaged`
  UnwindScenario scenario(BindRepeating(
      &CallThroughOtherLibrary, UnsafeDanglingUntriaged(other_library)));

  UnwindScenario::SampleEvents events;
  TargetThread target_thread(
      BindLambdaForTesting([&] { scenario.Execute(&events); }));
  target_thread.Start();
  events.ready_for_sample.Wait();

  WaitableEvent sampling_thread_completed(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  Profile profile;

  WaitableEvent stack_copied(WaitableEvent::ResetPolicy::MANUAL,
                             WaitableEvent::InitialState::NOT_SIGNALED);
  WaitableEvent start_stack_walk(WaitableEvent::ResetPolicy::MANUAL,
                                 WaitableEvent::InitialState::NOT_SIGNALED);
  StackCopiedSignaler test_delegate(&stack_copied, &start_stack_walk,
                                    wait_until_unloaded);
  StackSamplingProfiler profiler(
      target_thread.thread_token(), params,
      std::make_unique<TestProfileBuilder>(
          module_cache,
          BindLambdaForTesting(
              [&profile, &sampling_thread_completed](Profile result_profile) {
                profile = std::move(result_profile);
                sampling_thread_completed.Signal();
              })),
      CreateCoreUnwindersFactoryForTesting(module_cache), RepeatingClosure(),
      &test_delegate);

  profiler.Start();

  // Wait for the stack to be copied and the target thread to be resumed.
  stack_copied.Wait();

  // Cause the target thread to finish, so that it's no longer executing code in
  // the library we're about to unload.
  events.sample_finished.Signal();
  target_thread.Join();

  // Unload the library now that it's not being used.
  if (wait_until_unloaded)
    SynchronousUnloadNativeLibrary(other_library);
  else
    UnloadNativeLibrary(other_library);

  // Let the stack walk commence after unloading the library, if we're waiting
  // on that event.
  start_stack_walk.Signal();

  // Wait for the sampling thread to complete and fill out |profile|.
  sampling_thread_completed.Wait();

  // Look up the sample.
  ASSERT_EQ(1u, profile.samples.size());
  const std::vector<Frame>& sample = profile.samples[0];

  if (wait_until_unloaded) {
    // We expect the stack to look something like this, with the frame in the
    // now-unloaded library having a null module.
    //
    // ... WaitableEvent and system frames ...
    // WaitForSample()
    // TargetThread::OtherLibraryCallback
    // <frame in unloaded library>
    EXPECT_EQ(nullptr, sample.back().module)
        << "Stack:\n"
        << FormatSampleForDiagnosticOutput(sample);

    ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange()});
    ExpectStackDoesNotContain(sample,
                              {scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
  } else {
    // We didn't wait for the asynchronous unloading to complete, so the results
    // are non-deterministic: if the library finished unloading we should have
    // the same stack as |wait_until_unloaded|, if not we should have the full
    // stack. The important thing is that we should not crash.

    if (!sample.back().module) {
      // This is the same case as |wait_until_unloaded|.
      ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange()});
      ExpectStackDoesNotContain(sample,
                                {scenario.GetSetupFunctionAddressRange(),
                                 scenario.GetOuterFunctionAddressRange()});
      return;
    }

    ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                                 scenario.GetSetupFunctionAddressRange(),
                                 scenario.GetOuterFunctionAddressRange()});
  }
}

// Provide a suitable (and clean) environment for the tests below. All tests
// must use this class to ensure that proper clean-up is done and thus be
// usable in a later test.
class StackSamplingProfilerTest : public testing::Test {
 public:
  StackSamplingProfilerTest() = default;

  void SetUp() override {
    // The idle-shutdown time is too long for convenient (and accurate) testing.
    // That behavior is checked instead by artificially triggering it through
    // the TestPeer.
    StackSamplingProfiler::TestPeer::DisableIdleShutdown();
  }

  void TearDown() override {
    // Be a good citizen and clean up after ourselves. This also re-enables the
    // idle-shutdown behavior.
    StackSamplingProfiler::TestPeer::Reset();
  }

 protected:
  ModuleCache* module_cache() { return &module_cache_; }

 private:
  ModuleCache module_cache_;
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

// Checks that the basic expected information is present in sampled frames.
//
// macOS ASAN is not yet supported - crbug.com/718628.
//
// TODO(crbug.com/40702833): Enable this test again for Android with
// ASAN. This is now disabled because the android-asan bot fails.
//
// If we're running the ChromeOS unit tests on Linux, this test will never pass
// because Ubuntu's libc isn't compiled with frame pointers. Skip if not a real
// ChromeOS device.
#if (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_APPLE)) ||   \
    (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_ANDROID)) || \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
#define MAYBE_Basic DISABLED_Basic
#else
#define MAYBE_Basic Basic
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_Basic) {
  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));
  const std::vector<Frame>& sample = SampleScenario(&scenario, module_cache());

  // Check that all the modules are valid.
  for (const auto& frame : sample)
    EXPECT_NE(nullptr, frame.module);

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// A simple unwinder that always generates one frame then aborts the stack walk.
class TestAuxUnwinder : public Unwinder {
 public:
  TestAuxUnwinder(const Frame& frame_to_report,
                  base::RepeatingClosure add_initial_modules_callback)
      : frame_to_report_(frame_to_report),
        add_initial_modules_callback_(std::move(add_initial_modules_callback)) {
  }

  TestAuxUnwinder(const TestAuxUnwinder&) = delete;
  TestAuxUnwinder& operator=(const TestAuxUnwinder&) = delete;

  void InitializeModules() override {
    if (add_initial_modules_callback_)
      add_initial_modules_callback_.Run();
  }
  bool CanUnwindFrom(const Frame& current_frame) const override { return true; }

  UnwindResult TryUnwind(UnwinderStateCapture* capture_state,
                         RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) override {
    stack->push_back(frame_to_report_);
    return UnwindResult::kAborted;
  }

 private:
  const Frame frame_to_report_;
  base::RepeatingClosure add_initial_modules_callback_;
};

// Checks that the profiler handles stacks containing dynamically-allocated
// stack memory.
// macOS ASAN is not yet supported - crbug.com/718628.
// Android is not supported since Chrome unwind tables don't support dynamic
// frames.
// If we're running the ChromeOS unit tests on Linux, this test will never pass
// because Ubuntu's libc isn't compiled with frame pointers. Skip if not a real
// ChromeOS device.
#if (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_APPLE)) || \
    BUILDFLAG(IS_ANDROID) ||                               \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
#define MAYBE_Alloca DISABLED_Alloca
#else
#define MAYBE_Alloca Alloca
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_Alloca) {
  UnwindScenario scenario(BindRepeating(&CallWithAlloca));
  const std::vector<Frame>& sample = SampleScenario(&scenario, module_cache());

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that a stack that runs through another library produces a stack with
// the expected functions.
// macOS ASAN is not yet supported - crbug.com/718628.
// iOS chrome doesn't support loading native libraries.
// Android is not supported when EXCLUDE_UNWIND_TABLES |other_library| doesn't
// have unwind tables.
// TODO(crbug.com/40702833): Enable this test again for Android with
// ASAN. This is now disabled because the android-asan bot fails.
// If we're running the ChromeOS unit tests on Linux, this test will never pass
// because Ubuntu's libc isn't compiled with frame pointers. Skip if not a real
// ChromeOS device.
#if (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_APPLE)) ||         \
    BUILDFLAG(IS_IOS) ||                                           \
    (BUILDFLAG(IS_ANDROID) && BUILDFLAG(EXCLUDE_UNWIND_TABLES)) || \
    (BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)) ||       \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
#define MAYBE_OtherLibrary DISABLED_OtherLibrary
#else
#define MAYBE_OtherLibrary OtherLibrary
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_OtherLibrary) {
  ScopedNativeLibrary other_library(LoadOtherLibrary());
  UnwindScenario scenario(
      BindRepeating(&CallThroughOtherLibrary, Unretained(other_library.get())));
  const std::vector<Frame>& sample = SampleScenario(&scenario, module_cache());

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});
}

// Checks that a stack that runs through a library that is unloading produces a
// stack, and doesn't crash.
// Unloading is synchronous on the Mac, so this test is inapplicable.
// Android is not supported when EXCLUDE_UNWIND_TABLES |other_library| doesn't
// have unwind tables.
// TODO(crbug.com/40702833): Enable this test again for Android with
// ASAN. This is now disabled because the android-asan bot fails.
// If we're running the ChromeOS unit tests on Linux, this test will never pass
// because Ubuntu's libc isn't compiled with frame pointers. Skip if not a real
// ChromeOS device.
#if BUILDFLAG(IS_APPLE) ||                                         \
    (BUILDFLAG(IS_ANDROID) && BUILDFLAG(EXCLUDE_UNWIND_TABLES)) || \
    (BUILDFLAG(IS_ANDROID) && defined(ADDRESS_SANITIZER)) ||       \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
#define MAYBE_UnloadingLibrary DISABLED_UnloadingLibrary
#else
#define MAYBE_UnloadingLibrary UnloadingLibrary
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_UnloadingLibrary) {
  TestLibraryUnload(false, module_cache());
}

// Checks that a stack that runs through a library that has been unloaded
// produces a stack, and doesn't crash.
// macOS ASAN is not yet supported - crbug.com/718628.
// Android is not supported since modules are found before unwinding.
// If we're running the ChromeOS unit tests on Linux, this test will never pass
// because Ubuntu's libc isn't compiled with frame pointers. Skip if not a real
// ChromeOS device.
#if (defined(ADDRESS_SANITIZER) && BUILDFLAG(IS_APPLE)) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS) ||          \
    (BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_CHROMEOS_DEVICE))
#define MAYBE_UnloadedLibrary DISABLED_UnloadedLibrary
#else
#define MAYBE_UnloadedLibrary UnloadedLibrary
#endif
PROFILER_TEST_F(StackSamplingProfilerTest, MAYBE_UnloadedLibrary) {
  TestLibraryUnload(true, module_cache());
}

// Checks that a profiler can stop/destruct without ever having started.
PROFILER_TEST_F(StackSamplingProfilerTest, StopWithoutStarting) {
  WithTargetThread(BindLambdaForTesting(
      [this](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params;
        params.sampling_interval = Milliseconds(0);
        params.samples_per_profile = 1;

        Profile profile;
        WaitableEvent sampling_completed(
            WaitableEvent::ResetPolicy::MANUAL,
            WaitableEvent::InitialState::NOT_SIGNALED);

        StackSamplingProfiler profiler(
            target_thread_token, params,
            std::make_unique<TestProfileBuilder>(
                module_cache(),
                BindLambdaForTesting(
                    [&profile, &sampling_completed](Profile result_profile) {
                      profile = std::move(result_profile);
                      sampling_completed.Signal();
                    })),
            CreateCoreUnwindersFactoryForTesting(module_cache()));

        profiler.Stop();  // Constructed but never started.
        EXPECT_FALSE(sampling_completed.IsSignaled());
      }));
}

// Checks that its okay to stop a profiler before it finishes even when the
// sampling thread continues to run.
PROFILER_TEST_F(StackSamplingProfilerTest, StopSafely) {
  // Test delegate that counts samples.
  class SampleRecordedCounter : public StackSamplerTestDelegate {
   public:
    SampleRecordedCounter() = default;

    void OnPreStackWalk() override {
      AutoLock lock(lock_);
      ++count_;
    }

    size_t Get() {
      AutoLock lock(lock_);
      return count_;
    }

   private:
    Lock lock_;
    size_t count_ = 0;
  };

  WithTargetThread(
      BindLambdaForTesting([](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params[2];

        // Providing an initial delay makes it more likely that both will be
        // scheduled before either starts to run. Once started, samples will
        // run ordered by their scheduled, interleaved times regardless of
        // whatever interval the thread wakes up.
        params[0].initial_delay = Milliseconds(10);
        params[0].sampling_interval = Milliseconds(1);
        params[0].samples_per_profile = 100000;

        params[1].initial_delay = Milliseconds(10);
        params[1].sampling_interval = Milliseconds(1);
        params[1].samples_per_profile = 100000;

        SampleRecordedCounter samples_recorded[std::size(params)];
        ModuleCache module_cache1, module_cache2;
        TestProfilerInfo profiler_info0(target_thread_token, params[0],
                                        &module_cache1, &samples_recorded[0]);
        TestProfilerInfo profiler_info1(target_thread_token, params[1],
                                        &module_cache2, &samples_recorded[1]);

        profiler_info0.profiler.Start();
        profiler_info1.profiler.Start();

        // Wait for both to start accumulating samples. Using a WaitableEvent is
        // possible but gets complicated later on because there's no way of
        // knowing if 0 or 1 additional sample will be taken after Stop() and
        // thus no way of knowing how many Wait() calls to make on it.
        while (samples_recorded[0].Get() == 0 || samples_recorded[1].Get() == 0)
          PlatformThread::Sleep(Milliseconds(1));

        // Ensure that the first sampler can be safely stopped while the second
        // continues to run. The stopped first profiler will still have a
        // RecordSampleTask pending that will do nothing when executed because
        // the collection will have been removed by Stop().
        profiler_info0.profiler.Stop();
        profiler_info0.completed.Wait();
        size_t count0 = samples_recorded[0].Get();
        size_t count1 = samples_recorded[1].Get();

        // Waiting for the second sampler to collect a couple samples ensures
        // that the pending RecordSampleTask for the first has executed because
        // tasks are always ordered by their next scheduled time.
        while (samples_recorded[1].Get() < count1 + 2)
          PlatformThread::Sleep(Milliseconds(1));

        // Ensure that the first profiler didn't do anything since it was
        // stopped.
        EXPECT_EQ(count0, samples_recorded[0].Get());
      }));
}

// Checks that no sample are captured if the profiling is stopped during the
// initial delay.
PROFILER_TEST_F(StackSamplingProfilerTest, StopDuringInitialDelay) {
  SamplingParams params;
  params.initial_delay = Seconds(60);

  std::vector<std::vector<Frame>> samples =
      CaptureSamples(params, Milliseconds(0), module_cache());

  EXPECT_TRUE(samples.empty());
}

// Checks that tasks can be stopped before completion and incomplete samples are
// captured.
PROFILER_TEST_F(StackSamplingProfilerTest, StopDuringInterSampleInterval) {
  // Test delegate that counts samples.
  class SampleRecordedEvent : public StackSamplerTestDelegate {
   public:
    SampleRecordedEvent()
        : sample_recorded_(WaitableEvent::ResetPolicy::MANUAL,
                           WaitableEvent::InitialState::NOT_SIGNALED) {}

    void OnPreStackWalk() override { sample_recorded_.Signal(); }

    void WaitForSample() { sample_recorded_.Wait(); }

   private:
    WaitableEvent sample_recorded_;
  };

  WithTargetThread(BindLambdaForTesting(
      [this](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params;

        params.sampling_interval = AVeryLongTimeDelta();
        params.samples_per_profile = 2;

        SampleRecordedEvent samples_recorded;
        TestProfilerInfo profiler_info(target_thread_token, params,
                                       module_cache(), &samples_recorded);

        profiler_info.profiler.Start();

        // Wait for profiler to start accumulating samples.
        samples_recorded.WaitForSample();

        // Ensure that it can stop safely.
        profiler_info.profiler.Stop();
        profiler_info.completed.Wait();

        EXPECT_EQ(1u, profiler_info.profile.samples.size());
      }));
}

PROFILER_TEST_F(StackSamplingProfilerTest, GetNextSampleTime_NormalExecution) {
  const auto& GetNextSampleTime =
      StackSamplingProfiler::TestPeer::GetNextSampleTime;

  const TimeTicks scheduled_current_sample_time = TimeTicks::UnixEpoch();
  const TimeDelta sampling_interval = Milliseconds(10);

  // When executing the sample at exactly the scheduled time the next sample
  // should be one interval later.
  EXPECT_EQ(scheduled_current_sample_time + sampling_interval,
            GetNextSampleTime(scheduled_current_sample_time, sampling_interval,
                              scheduled_current_sample_time));

  // When executing the sample less than half an interval after the scheduled
  // time the next sample also should be one interval later.
  EXPECT_EQ(scheduled_current_sample_time + sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 0.4 * sampling_interval));

  // When executing the sample less than half an interval before the scheduled
  // time the next sample also should be one interval later. This is not
  // expected to occur in practice since delayed tasks never run early.
  EXPECT_EQ(scheduled_current_sample_time + sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time - 0.4 * sampling_interval));
}

PROFILER_TEST_F(StackSamplingProfilerTest, GetNextSampleTime_DelayedExecution) {
  const auto& GetNextSampleTime =
      StackSamplingProfiler::TestPeer::GetNextSampleTime;

  const TimeTicks scheduled_current_sample_time = TimeTicks::UnixEpoch();
  const TimeDelta sampling_interval = Milliseconds(10);

  // When executing the sample between 0.5 and 1.5 intervals after the scheduled
  // time the next sample should be two intervals later.
  EXPECT_EQ(scheduled_current_sample_time + 2 * sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 0.6 * sampling_interval));
  EXPECT_EQ(scheduled_current_sample_time + 2 * sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 1.0 * sampling_interval));
  EXPECT_EQ(scheduled_current_sample_time + 2 * sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 1.4 * sampling_interval));

  // Similarly when executing the sample between 9.5 and 10.5 intervals after
  // the scheduled time the next sample should be 11 intervals later.
  EXPECT_EQ(scheduled_current_sample_time + 11 * sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 9.6 * sampling_interval));
  EXPECT_EQ(scheduled_current_sample_time + 11 * sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 10.0 * sampling_interval));
  EXPECT_EQ(scheduled_current_sample_time + 11 * sampling_interval,
            GetNextSampleTime(
                scheduled_current_sample_time, sampling_interval,
                scheduled_current_sample_time + 10.4 * sampling_interval));
}

// Checks that we can destroy the profiler while profiling.
PROFILER_TEST_F(StackSamplingProfilerTest, DestroyProfilerWhileProfiling) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(10);

  Profile profile;
  WithTargetThread(BindLambdaForTesting([&, this](SamplingProfilerThreadToken
                                                      target_thread_token) {
    std::unique_ptr<StackSamplingProfiler> profiler;
    auto profile_builder = std::make_unique<TestProfileBuilder>(
        module_cache(),
        BindLambdaForTesting([&profile](Profile result_profile) {
          profile = std::move(result_profile);
        }));
    profiler = std::make_unique<StackSamplingProfiler>(
        target_thread_token, params, std::move(profile_builder),
        CreateCoreUnwindersFactoryForTesting(module_cache()));
    profiler->Start();
    profiler.reset();

    // Wait longer than a sample interval to catch any use-after-free actions by
    // the profiler thread.
    PlatformThread::Sleep(Milliseconds(50));
  }));
}

// Checks that the different profilers may be run.
PROFILER_TEST_F(StackSamplingProfilerTest, CanRunMultipleProfilers) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  std::vector<std::vector<Frame>> samples =
      CaptureSamples(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, samples.size());

  samples = CaptureSamples(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, samples.size());
}

// Checks that a sampler can be started while another is running.
PROFILER_TEST_F(StackSamplingProfilerTest, MultipleStart) {
  WithTargetThread(
      BindLambdaForTesting([](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params1;
        params1.initial_delay = AVeryLongTimeDelta();
        params1.samples_per_profile = 1;
        ModuleCache module_cache1;
        TestProfilerInfo profiler_info1(target_thread_token, params1,
                                        &module_cache1);

        SamplingParams params2;
        params2.sampling_interval = Milliseconds(1);
        params2.samples_per_profile = 1;
        ModuleCache module_cache2;
        TestProfilerInfo profiler_info2(target_thread_token, params2,
                                        &module_cache2);

        profiler_info1.profiler.Start();
        profiler_info2.profiler.Start();
        profiler_info2.completed.Wait();
        EXPECT_EQ(1u, profiler_info2.profile.samples.size());
      }));
}

// Checks that the profile duration and the sampling interval are calculated
// correctly. Also checks that RecordMetadata() is invoked each time a sample
// is recorded.
PROFILER_TEST_F(StackSamplingProfilerTest, ProfileGeneralInfo) {
  WithTargetThread(BindLambdaForTesting(
      [this](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params;
        params.sampling_interval = Milliseconds(1);
        params.samples_per_profile = 3;

        TestProfilerInfo profiler_info(target_thread_token, params,
                                       module_cache());

        profiler_info.profiler.Start();
        profiler_info.completed.Wait();
        EXPECT_EQ(3u, profiler_info.profile.samples.size());

        // The profile duration should be greater than the total sampling
        // intervals.
        EXPECT_GT(profiler_info.profile.profile_duration,
                  profiler_info.profile.sampling_period * 3);

        EXPECT_EQ(Milliseconds(1), profiler_info.profile.sampling_period);

        // The number of invocations of RecordMetadata() should be equal to the
        // number of samples recorded.
        EXPECT_EQ(3, profiler_info.profile.record_metadata_count);
      }));
}

// Checks that the sampling thread can shut down.
PROFILER_TEST_F(StackSamplingProfilerTest, SamplerIdleShutdown) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  std::vector<std::vector<Frame>> samples =
      CaptureSamples(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, samples.size());

  // Capture thread should still be running at this point.
  ASSERT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

  // Initiate an "idle" shutdown and ensure it happens. Idle-shutdown was
  // disabled by the test fixture so the test will fail due to a timeout if
  // it does not exit.
  StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(false);

  // While the shutdown has been initiated, the actual exit of the thread still
  // happens asynchronously. Watch until the thread actually exits. This test
  // will time-out in the case of failure.
  while (StackSamplingProfiler::TestPeer::IsSamplingThreadRunning())
    PlatformThread::Sleep(Milliseconds(1));
}

// Checks that additional requests will restart a stopped profiler.
PROFILER_TEST_F(StackSamplingProfilerTest,
                WillRestartSamplerAfterIdleShutdown) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  std::vector<std::vector<Frame>> samples =
      CaptureSamples(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, samples.size());

  // Capture thread should still be running at this point.
  ASSERT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

  // Post a ShutdownTask on the sampling thread which, when executed, will
  // mark the thread as EXITING and begin shut down of the thread.
  StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(false);

  // Ensure another capture will start the sampling thread and run.
  samples = CaptureSamples(params, AVeryLongTimeDelta(), module_cache());
  ASSERT_EQ(1u, samples.size());
  EXPECT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());
}

// Checks that it's safe to stop a task after it's completed and the sampling
// thread has shut-down for being idle.
PROFILER_TEST_F(StackSamplingProfilerTest, StopAfterIdleShutdown) {
  WithTargetThread(BindLambdaForTesting(
      [this](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params;

        params.sampling_interval = Milliseconds(1);
        params.samples_per_profile = 1;

        TestProfilerInfo profiler_info(target_thread_token, params,
                                       module_cache());

        profiler_info.profiler.Start();
        profiler_info.completed.Wait();

        // Capture thread should still be running at this point.
        ASSERT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

        // Perform an idle shutdown.
        StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(
            false);

        // Stop should be safe though its impossible to know at this moment if
        // the sampling thread has completely exited or will just "stop soon".
        profiler_info.profiler.Stop();
      }));
}

// Checks that profilers can run both before and after the sampling thread has
// started.
PROFILER_TEST_F(StackSamplingProfilerTest,
                ProfileBeforeAndAfterSamplingThreadRunning) {
  WithTargetThread(
      BindLambdaForTesting([](SamplingProfilerThreadToken target_thread_token) {
        ModuleCache module_cache1;
        ModuleCache module_cache2;

        std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos;
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/AVeryLongTimeDelta(),
                           /*samples_per_profile=*/1,
                           /*sampling_interval=*/Milliseconds(1)},
            &module_cache1));
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/Milliseconds(0),
                           /*samples_per_profile=*/1,
                           /*sampling_interval=*/Milliseconds(1)},
            &module_cache2));

        // First profiler is started when there has never been a sampling
        // thread.
        EXPECT_FALSE(
            StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());
        profiler_infos[0]->profiler.Start();
        // Second profiler is started when sampling thread is already running.
        EXPECT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());
        profiler_infos[1]->profiler.Start();

        // Only the second profiler should finish before test times out.
        size_t completed_profiler = WaitForSamplingComplete(profiler_infos);
        EXPECT_EQ(1U, completed_profiler);
      }));
}

// Checks that an idle-shutdown task will abort if a new profiler starts
// between when it was posted and when it runs.
PROFILER_TEST_F(StackSamplingProfilerTest, IdleShutdownAbort) {
  WithTargetThread(BindLambdaForTesting(
      [this](SamplingProfilerThreadToken target_thread_token) {
        SamplingParams params;

        params.sampling_interval = Milliseconds(1);
        params.samples_per_profile = 1;

        TestProfilerInfo profiler_info(target_thread_token, params,
                                       module_cache());

        profiler_info.profiler.Start();
        profiler_info.completed.Wait();
        EXPECT_EQ(1u, profiler_info.profile.samples.size());

        // Perform an idle shutdown but simulate that a new capture is started
        // before it can actually run.
        StackSamplingProfiler::TestPeer::PerformSamplingThreadIdleShutdown(
            true);

        // Though the shutdown-task has been executed, any actual exit of the
        // thread is asynchronous so there is no way to detect that *didn't*
        // exit except to wait a reasonable amount of time and then check. Since
        // the thread was just running ("perform" blocked until it was), it
        // should finish almost immediately and without any waiting for tasks or
        // events.
        PlatformThread::Sleep(Milliseconds(200));
        EXPECT_TRUE(StackSamplingProfiler::TestPeer::IsSamplingThreadRunning());

        // Ensure that it's still possible to run another sampler.
        TestProfilerInfo another_info(target_thread_token, params,
                                      module_cache());
        another_info.profiler.Start();
        another_info.completed.Wait();
        EXPECT_EQ(1u, another_info.profile.samples.size());
      }));
}

// Checks that synchronized multiple sampling requests execute in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, ConcurrentProfiling_InSync) {
  WithTargetThread(
      BindLambdaForTesting([](SamplingProfilerThreadToken target_thread_token) {
        ModuleCache module_cache1;
        ModuleCache module_cache2;

        // Providing an initial delay makes it more likely that both will be
        // scheduled before either starts to run. Once started, samples will
        // run ordered by their scheduled, interleaved times regardless of
        // whatever interval the thread wakes up. Thus, total execution time
        // will be 10ms (delay) + 10x1ms (sampling) + 1/2 timer minimum
        // interval.
        std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos;
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/Milliseconds(10),
                           /*samples_per_profile=*/9,
                           /*sampling_interval=*/Milliseconds(1)},
            &module_cache1));
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/Milliseconds(11),
                           /*samples_per_profile=*/8,
                           /*sampling_interval=*/Milliseconds(1)},
            &module_cache2));

        profiler_infos[0]->profiler.Start();
        profiler_infos[1]->profiler.Start();

        // Wait for one profiler to finish.
        size_t completed_profiler = WaitForSamplingComplete(profiler_infos);

        size_t other_profiler = 1 - completed_profiler;
        // Wait for the other profiler to finish.
        profiler_infos[other_profiler]->completed.Wait();

        // Ensure each got the correct number of samples.
        EXPECT_EQ(9u, profiler_infos[0]->profile.samples.size());
        EXPECT_EQ(8u, profiler_infos[1]->profile.samples.size());
      }));
}

// Checks that several mixed sampling requests execute in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, ConcurrentProfiling_Mixed) {
  WithTargetThread(
      BindLambdaForTesting([](SamplingProfilerThreadToken target_thread_token) {
        std::vector<ModuleCache> module_caches(3);

        std::vector<std::unique_ptr<TestProfilerInfo>> profiler_infos;
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/Milliseconds(8),
                           /*samples_per_profile=*/10,
                           /*sampling_interval=*/Milliseconds(4)},
            &module_caches[0]));
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/Milliseconds(9),
                           /*samples_per_profile=*/10,
                           /*sampling_interval=*/Milliseconds(3)},
            &module_caches[1]));
        profiler_infos.push_back(std::make_unique<TestProfilerInfo>(
            target_thread_token,
            SamplingParams{/*initial_delay=*/Milliseconds(10),
                           /*samples_per_profile=*/10,
                           /*sampling_interval=*/Milliseconds(2)},
            &module_caches[2]));

        for (auto& i : profiler_infos) {
          i->profiler.Start();
        }

        // Wait for one profiler to finish.
        size_t completed_profiler = WaitForSamplingComplete(profiler_infos);
        EXPECT_EQ(10u,
                  profiler_infos[completed_profiler]->profile.samples.size());
        // Stop and destroy all profilers, always in the same order. Don't
        // crash.
        for (auto& i : profiler_infos) {
          i->profiler.Stop();
        }
        for (auto& i : profiler_infos) {
          i.reset();
        }
      }));
}

// Checks that different threads can be sampled in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, MultipleSampledThreads) {
  UnwindScenario scenario1(BindRepeating(&CallWithPlainFunction));
  UnwindScenario::SampleEvents events1;
  TargetThread target_thread1(
      BindLambdaForTesting([&] { scenario1.Execute(&events1); }));
  target_thread1.Start();
  events1.ready_for_sample.Wait();

  UnwindScenario scenario2(BindRepeating(&CallWithPlainFunction));
  UnwindScenario::SampleEvents events2;
  TargetThread target_thread2(
      BindLambdaForTesting([&] { scenario2.Execute(&events2); }));
  target_thread2.Start();
  events2.ready_for_sample.Wait();

  // Providing an initial delay makes it more likely that both will be
  // scheduled before either starts to run. Once started, samples will
  // run ordered by their scheduled, interleaved times regardless of
  // whatever interval the thread wakes up.
  SamplingParams params1, params2;
  params1.initial_delay = Milliseconds(10);
  params1.sampling_interval = Milliseconds(1);
  params1.samples_per_profile = 9;
  params2.initial_delay = Milliseconds(10);
  params2.sampling_interval = Milliseconds(1);
  params2.samples_per_profile = 8;

  Profile profile1, profile2;
  ModuleCache module_cache1, module_cache2;

  WaitableEvent sampling_thread_completed1(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  StackSamplingProfiler profiler1(
      target_thread1.thread_token(), params1,
      std::make_unique<TestProfileBuilder>(
          &module_cache1,
          BindLambdaForTesting(
              [&profile1, &sampling_thread_completed1](Profile result_profile) {
                profile1 = std::move(result_profile);
                sampling_thread_completed1.Signal();
              })),
      CreateCoreUnwindersFactoryForTesting(&module_cache1));

  WaitableEvent sampling_thread_completed2(
      WaitableEvent::ResetPolicy::MANUAL,
      WaitableEvent::InitialState::NOT_SIGNALED);
  StackSamplingProfiler profiler2(
      target_thread2.thread_token(), params2,
      std::make_unique<TestProfileBuilder>(
          &module_cache2,
          BindLambdaForTesting(
              [&profile2, &sampling_thread_completed2](Profile result_profile) {
                profile2 = std::move(result_profile);
                sampling_thread_completed2.Signal();
              })),
      CreateCoreUnwindersFactoryForTesting(&module_cache2));

  // Finally the real work.
  profiler1.Start();
  profiler2.Start();
  sampling_thread_completed1.Wait();
  sampling_thread_completed2.Wait();
  EXPECT_EQ(9u, profile1.samples.size());
  EXPECT_EQ(8u, profile2.samples.size());

  events1.sample_finished.Signal();
  events2.sample_finished.Signal();
  target_thread1.Join();
  target_thread2.Join();
}

// A simple thread that runs a profiler on another thread.
class ProfilerThread : public SimpleThread {
 public:
  ProfilerThread(const std::string& name,
                 SamplingProfilerThreadToken thread_token,
                 const SamplingParams& params,
                 ModuleCache* module_cache)
      : SimpleThread(name, Options()),
        run_(WaitableEvent::ResetPolicy::MANUAL,
             WaitableEvent::InitialState::NOT_SIGNALED),
        completed_(WaitableEvent::ResetPolicy::MANUAL,
                   WaitableEvent::InitialState::NOT_SIGNALED),
        profiler_(thread_token,
                  params,
                  std::make_unique<TestProfileBuilder>(
                      module_cache,
                      BindLambdaForTesting([this](Profile result_profile) {
                        profile_ = std::move(result_profile);
                        completed_.Signal();
                      })),
                  CreateCoreUnwindersFactoryForTesting(module_cache)) {}
  void Run() override {
    run_.Wait();
    profiler_.Start();
  }

  void Go() { run_.Signal(); }

  void Wait() { completed_.Wait(); }

  Profile& profile() { return profile_; }

 private:
  WaitableEvent run_;

  Profile profile_;
  WaitableEvent completed_;
  StackSamplingProfiler profiler_;
};

// Checks that different threads can run samplers in parallel.
PROFILER_TEST_F(StackSamplingProfilerTest, MultipleProfilerThreads) {
  WithTargetThread(
      BindLambdaForTesting([](SamplingProfilerThreadToken target_thread_token) {
        // Providing an initial delay makes it more likely that both will be
        // scheduled before either starts to run. Once started, samples will
        // run ordered by their scheduled, interleaved times regardless of
        // whatever interval the thread wakes up.
        SamplingParams params1, params2;
        params1.initial_delay = Milliseconds(10);
        params1.sampling_interval = Milliseconds(1);
        params1.samples_per_profile = 9;
        params2.initial_delay = Milliseconds(10);
        params2.sampling_interval = Milliseconds(1);
        params2.samples_per_profile = 8;

        // Start the profiler threads and give them a moment to get going.
        ModuleCache module_cache1;
        ProfilerThread profiler_thread1("profiler1", target_thread_token,
                                        params1, &module_cache1);
        ModuleCache module_cache2;
        ProfilerThread profiler_thread2("profiler2", target_thread_token,
                                        params2, &module_cache2);
        profiler_thread1.Start();
        profiler_thread2.Start();
        PlatformThread::Sleep(Milliseconds(10));

        // This will (approximately) synchronize the two threads.
        profiler_thread1.Go();
        profiler_thread2.Go();

        // Wait for them both to finish and validate collection.
        profiler_thread1.Wait();
        profiler_thread2.Wait();
        EXPECT_EQ(9u, profiler_thread1.profile().samples.size());
        EXPECT_EQ(8u, profiler_thread2.profile().samples.size());

        profiler_thread1.Join();
        profiler_thread2.Join();
      }));
}

PROFILER_TEST_F(StackSamplingProfilerTest, AddAuxUnwinder_BeforeStart) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  int add_initial_modules_invocation_count = 0;
  const auto add_initial_modules_callback =
      [&add_initial_modules_invocation_count] {
        ++add_initial_modules_invocation_count;
      };

  Profile profile;
  WithTargetThread(
      &scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            WaitableEvent sampling_thread_completed(
                WaitableEvent::ResetPolicy::MANUAL,
                WaitableEvent::InitialState::NOT_SIGNALED);
            StackSamplingProfiler profiler(
                target_thread_token, params,
                std::make_unique<TestProfileBuilder>(
                    module_cache(),
                    BindLambdaForTesting([&profile, &sampling_thread_completed](
                                             Profile result_profile) {
                      profile = std::move(result_profile);
                      sampling_thread_completed.Signal();
                    })),
                CreateCoreUnwindersFactoryForTesting(module_cache()));
            profiler.AddAuxUnwinder(std::make_unique<TestAuxUnwinder>(
                Frame(23, nullptr),
                BindLambdaForTesting(add_initial_modules_callback)));
            profiler.Start();
            sampling_thread_completed.Wait();
          }));

  ASSERT_EQ(1, add_initial_modules_invocation_count);

  // The sample should have one frame from the context values aFFnd one from the
  // TestAuxUnwinder.
  ASSERT_EQ(1u, profile.samples.size());
  const std::vector<Frame>& frames = profile.samples[0];

  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(23u, frames[1].instruction_pointer);
  EXPECT_EQ(nullptr, frames[1].module);
}

PROFILER_TEST_F(StackSamplingProfilerTest, AddAuxUnwinder_AfterStart) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(10);
  params.samples_per_profile = 2;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  int add_initial_modules_invocation_count = 0;
  const auto add_initial_modules_callback =
      [&add_initial_modules_invocation_count] {
        ++add_initial_modules_invocation_count;
      };

  Profile profile;
  WithTargetThread(
      &scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            WaitableEvent sampling_thread_completed(
                WaitableEvent::ResetPolicy::MANUAL,
                WaitableEvent::InitialState::NOT_SIGNALED);
            StackSamplingProfiler profiler(
                target_thread_token, params,
                std::make_unique<TestProfileBuilder>(
                    module_cache(),
                    BindLambdaForTesting([&profile, &sampling_thread_completed](
                                             Profile result_profile) {
                      profile = std::move(result_profile);
                      sampling_thread_completed.Signal();
                    })),
                CreateCoreUnwindersFactoryForTesting(module_cache()));
            profiler.Start();
            profiler.AddAuxUnwinder(std::make_unique<TestAuxUnwinder>(
                Frame(23, nullptr),
                BindLambdaForTesting(add_initial_modules_callback)));
            sampling_thread_completed.Wait();
          }));

  ASSERT_EQ(1, add_initial_modules_invocation_count);

  // The sample should have one frame from the context values and one from the
  // TestAuxUnwinder.
  ASSERT_EQ(2u, profile.samples.size());

  // Whether the aux unwinder is available for the first sample is racy, so rely
  // on the second sample.
  const std::vector<Frame>& frames = profile.samples[1];
  ASSERT_EQ(2u, frames.size());
  EXPECT_EQ(23u, frames[1].instruction_pointer);
  EXPECT_EQ(nullptr, frames[1].module);
}

PROFILER_TEST_F(StackSamplingProfilerTest, AddAuxUnwinder_AfterStop) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  Profile profile;
  WithTargetThread(
      &scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            WaitableEvent sampling_thread_completed(
                WaitableEvent::ResetPolicy::MANUAL,
                WaitableEvent::InitialState::NOT_SIGNALED);
            StackSamplingProfiler profiler(
                target_thread_token, params,
                std::make_unique<TestProfileBuilder>(
                    module_cache(),
                    BindLambdaForTesting([&profile, &sampling_thread_completed](
                                             Profile result_profile) {
                      profile = std::move(result_profile);
                      sampling_thread_completed.Signal();
                    })),
                CreateCoreUnwindersFactoryForTesting(module_cache()));
            profiler.Start();
            profiler.Stop();
            profiler.AddAuxUnwinder(std::make_unique<TestAuxUnwinder>(
                Frame(23, nullptr), base::RepeatingClosure()));
            sampling_thread_completed.Wait();
          }));

  // The AuxUnwinder should be accepted without error. It will have no effect
  // since the collection has stopped.
}

// Checks that requests to apply metadata to past samples are passed on to the
// profile builder.
PROFILER_TEST_F(StackSamplingProfilerTest,
                ApplyMetadataToPastSamples_PassedToProfileBuilder) {
  // Runs the passed closure on the profiler thread after a sample is taken.
  class PostSampleInvoker : public StackSamplerTestDelegate {
   public:
    explicit PostSampleInvoker(RepeatingClosure post_sample_closure)
        : post_sample_closure_(std::move(post_sample_closure)) {}

    void OnPreStackWalk() override { post_sample_closure_.Run(); }

   private:
    RepeatingClosure post_sample_closure_;
  };

  // Thread-safe representation of the times that samples were taken.
  class SynchronizedSampleTimes {
   public:
    void AddNow() {
      AutoLock lock(lock_);
      times_.push_back(TimeTicks::Now());
    }

    std::vector<TimeTicks> GetTimes() {
      AutoLock lock(lock_);
      return times_;
    }

   private:
    Lock lock_;
    std::vector<TimeTicks> times_;
  };

  SamplingParams params;
  params.sampling_interval = Milliseconds(10);
  // 10,000 samples ensures the profiler continues running until manually
  // stopped, after applying metadata.
  params.samples_per_profile = 10000;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  std::vector<TimeTicks> sample_times;
  Profile profile;
  WithTargetThread(
      &scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            SynchronizedSampleTimes synchronized_sample_times;
            WaitableEvent sample_seen(WaitableEvent::ResetPolicy::AUTOMATIC);
            PostSampleInvoker post_sample_invoker(BindLambdaForTesting([&] {
              synchronized_sample_times.AddNow();
              sample_seen.Signal();
            }));

            StackSamplingProfiler profiler(
                target_thread_token, params,
                std::make_unique<TestProfileBuilder>(
                    module_cache(),
                    BindLambdaForTesting([&profile](Profile result_profile) {
                      profile = std::move(result_profile);
                    })),
                CreateCoreUnwindersFactoryForTesting(module_cache()),
                RepeatingClosure(), &post_sample_invoker);
            profiler.Start();
            // Wait for 5 samples to be collected.
            for (int i = 0; i < 5; ++i)
              sample_seen.Wait();
            sample_times = synchronized_sample_times.GetTimes();
            // Record metadata on past samples, with and without a key value.
            // The range [times[1], times[3]] is guaranteed to include only
            // samples 2 and 3, and likewise [times[2], times[4]] is guaranteed
            // to include only samples 3 and 4.
            ApplyMetadataToPastSamples(sample_times[1], sample_times[3],
                                       "TestMetadata1", 10,
                                       base::SampleMetadataScope::kProcess);
            ApplyMetadataToPastSamples(sample_times[2], sample_times[4],
                                       "TestMetadata2", 100, 11,
                                       base::SampleMetadataScope::kProcess);
            profiler.Stop();
          }));

  ASSERT_EQ(2u, profile.retrospective_metadata.size());

  const RetrospectiveMetadata& metadata1 = profile.retrospective_metadata[0];
  EXPECT_EQ(sample_times[1], metadata1.period_start);
  EXPECT_EQ(sample_times[3], metadata1.period_end);
  EXPECT_EQ(HashMetricName("TestMetadata1"), metadata1.item.name_hash);
  EXPECT_FALSE(metadata1.item.key.has_value());
  EXPECT_EQ(10, metadata1.item.value);

  const RetrospectiveMetadata& metadata2 = profile.retrospective_metadata[1];
  EXPECT_EQ(sample_times[2], metadata2.period_start);
  EXPECT_EQ(sample_times[4], metadata2.period_end);
  EXPECT_EQ(HashMetricName("TestMetadata2"), metadata2.item.name_hash);
  ASSERT_TRUE(metadata2.item.key.has_value());
  EXPECT_EQ(100, *metadata2.item.key);
  EXPECT_EQ(11, metadata2.item.value);
}

PROFILER_TEST_F(
    StackSamplingProfilerTest,
    ApplyMetadataToPastSamples_PassedToProfileBuilder_MultipleCollections) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(10);
  // 10,000 samples ensures the profiler continues running until manually
  // stopped, after applying metadata.
  params.samples_per_profile = 10000;
  ModuleCache module_cache1, module_cache2;

  WaitableEvent profiler1_started;
  WaitableEvent profiler2_started;
  WaitableEvent profiler1_metadata_applied;
  WaitableEvent profiler2_metadata_applied;

  Profile profile1;
  WaitableEvent sampling_completed1;
  TargetThread target_thread1(BindLambdaForTesting([&] {
    StackSamplingProfiler profiler1(
        target_thread1.thread_token(), params,
        std::make_unique<TestProfileBuilder>(
            &module_cache1, BindLambdaForTesting([&](Profile result_profile) {
              profile1 = std::move(result_profile);
              sampling_completed1.Signal();
            })),
        CreateCoreUnwindersFactoryForTesting(&module_cache1),
        RepeatingClosure());
    profiler1.Start();
    profiler1_started.Signal();
    profiler2_started.Wait();

    // Record metadata on past samples only for this thread. The time range
    // shouldn't affect the outcome, it should always be passed to the
    // ProfileBuilder.
    ApplyMetadataToPastSamples(TimeTicks(), TimeTicks::Now(), "TestMetadata1",
                               10, 10, SampleMetadataScope::kThread);

    profiler1_metadata_applied.Signal();
    profiler2_metadata_applied.Wait();
    profiler1.Stop();
  }));
  target_thread1.Start();

  Profile profile2;
  WaitableEvent sampling_completed2;
  TargetThread target_thread2(BindLambdaForTesting([&] {
    StackSamplingProfiler profiler2(
        target_thread2.thread_token(), params,
        std::make_unique<TestProfileBuilder>(
            &module_cache2, BindLambdaForTesting([&](Profile result_profile) {
              profile2 = std::move(result_profile);
              sampling_completed2.Signal();
            })),
        CreateCoreUnwindersFactoryForTesting(&module_cache2),
        RepeatingClosure());
    profiler2.Start();
    profiler2_started.Signal();
    profiler1_started.Wait();

    // Record metadata on past samples only for this thread.
    ApplyMetadataToPastSamples(TimeTicks(), TimeTicks::Now(), "TestMetadata2",
                               20, 20, SampleMetadataScope::kThread);

    profiler2_metadata_applied.Signal();
    profiler1_metadata_applied.Wait();
    profiler2.Stop();
  }));
  target_thread2.Start();

  target_thread1.Join();
  target_thread2.Join();

  // Wait for the profile to be captured before checking expectations.
  sampling_completed1.Wait();
  sampling_completed2.Wait();

  ASSERT_EQ(1u, profile1.retrospective_metadata.size());
  ASSERT_EQ(1u, profile2.retrospective_metadata.size());

  {
    const RetrospectiveMetadata& metadata1 = profile1.retrospective_metadata[0];
    EXPECT_EQ(HashMetricName("TestMetadata1"), metadata1.item.name_hash);
    ASSERT_TRUE(metadata1.item.key.has_value());
    EXPECT_EQ(10, *metadata1.item.key);
    EXPECT_EQ(10, metadata1.item.value);
  }
  {
    const RetrospectiveMetadata& metadata2 = profile2.retrospective_metadata[0];
    EXPECT_EQ(HashMetricName("TestMetadata2"), metadata2.item.name_hash);
    ASSERT_TRUE(metadata2.item.key.has_value());
    EXPECT_EQ(20, *metadata2.item.key);
    EXPECT_EQ(20, metadata2.item.value);
  }
}

// Checks that requests to add profile metadata are passed on to the profile
// builder.
PROFILER_TEST_F(StackSamplingProfilerTest,
                AddProfileMetadata_PassedToProfileBuilder) {
  // Runs the passed closure on the profiler thread after a sample is taken.
  class PostSampleInvoker : public StackSamplerTestDelegate {
   public:
    explicit PostSampleInvoker(RepeatingClosure post_sample_closure)
        : post_sample_closure_(std::move(post_sample_closure)) {}

    void OnPreStackWalk() override { post_sample_closure_.Run(); }

   private:
    RepeatingClosure post_sample_closure_;
  };

  SamplingParams params;
  params.sampling_interval = Milliseconds(10);
  // 10,000 samples ensures the profiler continues running until manually
  // stopped.
  params.samples_per_profile = 10000;

  UnwindScenario scenario(BindRepeating(&CallWithPlainFunction));

  Profile profile;
  WithTargetThread(
      &scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            WaitableEvent sample_seen(WaitableEvent::ResetPolicy::AUTOMATIC);
            PostSampleInvoker post_sample_invoker(
                BindLambdaForTesting([&] { sample_seen.Signal(); }));

            StackSamplingProfiler profiler(
                target_thread_token, params,
                std::make_unique<TestProfileBuilder>(
                    module_cache(),
                    BindLambdaForTesting([&profile](Profile result_profile) {
                      profile = std::move(result_profile);
                    })),
                CreateCoreUnwindersFactoryForTesting(module_cache()),
                RepeatingClosure(), &post_sample_invoker);
            profiler.Start();
            sample_seen.Wait();
            AddProfileMetadata("TestMetadata", 1, 2,
                               SampleMetadataScope::kProcess);
            profiler.Stop();
          }));

  ASSERT_EQ(1u, profile.profile_metadata.size());
  const MetadataRecorder::Item& item = profile.profile_metadata[0];
  EXPECT_EQ(HashMetricName("TestMetadata"), item.name_hash);
  EXPECT_EQ(1, *item.key);
  EXPECT_EQ(2, item.value);
}

PROFILER_TEST_F(StackSamplingProfilerTest,
                AddProfileMetadata_PassedToProfileBuilder_MultipleCollections) {
  SamplingParams params;
  params.sampling_interval = Milliseconds(10);
  // 10,000 samples ensures the profiler continues running until manually
  // stopped.
  params.samples_per_profile = 10000;
  ModuleCache module_cache1, module_cache2;

  WaitableEvent profiler1_started;
  WaitableEvent profiler2_started;
  WaitableEvent profiler1_metadata_applied;
  WaitableEvent profiler2_metadata_applied;

  Profile profile1;
  WaitableEvent sampling_completed1;
  TargetThread target_thread1(BindLambdaForTesting([&] {
    StackSamplingProfiler profiler1(
        target_thread1.thread_token(), params,
        std::make_unique<TestProfileBuilder>(
            &module_cache1, BindLambdaForTesting([&](Profile result_profile) {
              profile1 = std::move(result_profile);
              sampling_completed1.Signal();
            })),
        CreateCoreUnwindersFactoryForTesting(&module_cache1),
        RepeatingClosure());
    profiler1.Start();
    profiler1_started.Signal();
    profiler2_started.Wait();

    AddProfileMetadata("TestMetadata1", 1, 2, SampleMetadataScope::kThread);

    profiler1_metadata_applied.Signal();
    profiler2_metadata_applied.Wait();
    profiler1.Stop();
  }));
  target_thread1.Start();

  Profile profile2;
  WaitableEvent sampling_completed2;
  TargetThread target_thread2(BindLambdaForTesting([&] {
    StackSamplingProfiler profiler2(
        target_thread2.thread_token(), params,
        std::make_unique<TestProfileBuilder>(
            &module_cache2, BindLambdaForTesting([&](Profile result_profile) {
              profile2 = std::move(result_profile);
              sampling_completed2.Signal();
            })),
        CreateCoreUnwindersFactoryForTesting(&module_cache2),
        RepeatingClosure());
    profiler2.Start();
    profiler2_started.Signal();
    profiler1_started.Wait();

    AddProfileMetadata("TestMetadata2", 11, 12, SampleMetadataScope::kThread);

    profiler2_metadata_applied.Signal();
    profiler1_metadata_applied.Wait();
    profiler2.Stop();
  }));
  target_thread2.Start();

  target_thread1.Join();
  target_thread2.Join();

  // Wait for the profile to be captured before checking expectations.
  sampling_completed1.Wait();
  sampling_completed2.Wait();

  ASSERT_EQ(1u, profile1.profile_metadata.size());
  ASSERT_EQ(1u, profile2.profile_metadata.size());

  {
    const MetadataRecorder::Item& item = profile1.profile_metadata[0];
    EXPECT_EQ(HashMetricName("TestMetadata1"), item.name_hash);
    ASSERT_TRUE(item.key.has_value());
    EXPECT_EQ(1, *item.key);
    EXPECT_EQ(2, item.value);
  }
  {
    const MetadataRecorder::Item& item = profile2.profile_metadata[0];
    EXPECT_EQ(HashMetricName("TestMetadata2"), item.name_hash);
    ASSERT_TRUE(item.key.has_value());
    EXPECT_EQ(11, *item.key);
    EXPECT_EQ(12, item.value);
  }
}

}  // namespace base
