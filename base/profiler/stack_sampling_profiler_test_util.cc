// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/profiler/stack_sampling_profiler_test_util.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/profiler/native_unwinder_android_map_delegate.h"
#include "base/profiler/native_unwinder_android_memory_regions_map.h"
#include "base/profiler/profiler_buildflags.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/profiler/unwinder.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
#include "base/android/apk_assets.h"
#include "base/android/library_loader/anchor_functions.h"
#include "base/files/memory_mapped_file.h"
#include "base/no_destructor.h"
#include "base/profiler/chrome_unwinder_android.h"
#include "base/profiler/native_unwinder_android.h"
#endif

#if BUILDFLAG(IS_WIN)
// Windows doesn't provide an alloca function like Linux does.
// Fortunately, it provides _alloca, which functions identically.
#include <malloc.h>
#define alloca _alloca
#else
#include <alloca.h>
#endif

extern "C" {
// The address of |__executable_start| gives the start address of the
// executable or shared library. This value is used to find the offset address
// of the instruction in binary from PC.
extern char __executable_start;
}

namespace base {

namespace {

// A profile builder for test use that expects to receive exactly one sample.
class TestProfileBuilder : public ProfileBuilder {
 public:
  // The callback is passed the last sample recorded.
  using CompletedCallback = OnceCallback<void(std::vector<Frame>)>;

  TestProfileBuilder(ModuleCache* module_cache, CompletedCallback callback)
      : module_cache_(module_cache), callback_(std::move(callback)) {}

  ~TestProfileBuilder() override = default;

  TestProfileBuilder(const TestProfileBuilder&) = delete;
  TestProfileBuilder& operator=(const TestProfileBuilder&) = delete;

  // ProfileBuilder:
  ModuleCache* GetModuleCache() override { return module_cache_; }
  void RecordMetadata(
      const MetadataRecorder::MetadataProvider& metadata_provider) override {}

  void OnSampleCompleted(std::vector<Frame> sample,
                         TimeTicks sample_timestamp) override {
    EXPECT_TRUE(sample_.empty());
    sample_ = std::move(sample);
  }

  void OnProfileCompleted(TimeDelta profile_duration,
                          TimeDelta sampling_period) override {
    EXPECT_FALSE(sample_.empty());
    std::move(callback_).Run(std::move(sample_));
  }

 private:
  const raw_ptr<ModuleCache> module_cache_;
  CompletedCallback callback_;
  std::vector<Frame> sample_;
};

// The function to be executed by the code in the other library.
void OtherLibraryCallback(void* arg) {
  OnceClosure* wait_for_sample = static_cast<OnceClosure*>(arg);

  std::move(*wait_for_sample).Run();

  // Prevent tail call.
  [[maybe_unused]] volatile int i = 0;
}

#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
class NativeUnwinderAndroidMapDelegateForTesting
    : public NativeUnwinderAndroidMapDelegate {
 public:
  explicit NativeUnwinderAndroidMapDelegateForTesting(
      std::unique_ptr<NativeUnwinderAndroidMemoryRegionsMap> memory_regions_map)
      : memory_regions_map_(std::move(memory_regions_map)) {}

  NativeUnwinderAndroidMemoryRegionsMap* GetMapReference() override {
    return memory_regions_map_.get();
  }
  void ReleaseMapReference() override {}

 private:
  const std::unique_ptr<NativeUnwinderAndroidMemoryRegionsMap>
      memory_regions_map_;
};

// `map_delegate` should outlive the unwinder instance, so we cannot make a
// derived `NativeUnwinderAndroidForTesting` to own the `map_delegate`, as
// the base class outlives the derived class.
NativeUnwinderAndroidMapDelegateForTesting* GetMapDelegateForTesting() {
  static base::NoDestructor<NativeUnwinderAndroidMapDelegateForTesting>
      map_delegate(NativeUnwinderAndroid::CreateMemoryRegionsMap());
  return map_delegate.get();
}

std::unique_ptr<NativeUnwinderAndroid> CreateNativeUnwinderAndroidForTesting(
    uintptr_t exclude_module_with_base_address) {
  return std::make_unique<NativeUnwinderAndroid>(
      exclude_module_with_base_address, GetMapDelegateForTesting());
}

std::unique_ptr<Unwinder> CreateChromeUnwinderAndroidForTesting(
    uintptr_t chrome_module_base_address) {
  static constexpr char kCfiFileName[] = "assets/unwind_cfi_32_v2";

  // The wrapper class ensures that `MemoryMappedFile` has the same lifetime
  // as the unwinder.
  class ChromeUnwinderAndroidForTesting : public ChromeUnwinderAndroid {
   public:
    ChromeUnwinderAndroidForTesting(std::unique_ptr<MemoryMappedFile> cfi_file,
                                    const ChromeUnwindInfoAndroid& unwind_info,
                                    uintptr_t chrome_module_base_address,
                                    uintptr_t text_section_start_address)
        : ChromeUnwinderAndroid(unwind_info,
                                chrome_module_base_address,
                                text_section_start_address),
          cfi_file_(std::move(cfi_file)) {}
    ~ChromeUnwinderAndroidForTesting() override = default;

   private:
    std::unique_ptr<MemoryMappedFile> cfi_file_;
  };

  MemoryMappedFile::Region cfi_region;
  int fd = base::android::OpenApkAsset(kCfiFileName, &cfi_region);
  DCHECK_GT(fd, 0);
  auto cfi_file = std::make_unique<MemoryMappedFile>();
  bool ok = cfi_file->Initialize(base::File(fd), cfi_region);
  DCHECK(ok);
  return std::make_unique<ChromeUnwinderAndroidForTesting>(
      std::move(cfi_file),
      base::CreateChromeUnwindInfoAndroid(
          {cfi_file->data(), cfi_file->length()}),
      chrome_module_base_address,
      /* text_section_start_address= */ base::android::kStartOfText);
}
#endif  // #if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)

}  // namespace

TargetThread::TargetThread(OnceClosure to_run) : to_run_(std::move(to_run)) {}

TargetThread::~TargetThread() = default;

void TargetThread::Start() {
  EXPECT_TRUE(PlatformThread::Create(0, this, &target_thread_handle_));
}

void TargetThread::Join() {
  PlatformThread::Join(target_thread_handle_);
}

void TargetThread::ThreadMain() {
  thread_token_ = GetSamplingProfilerCurrentThreadToken();
  std::move(to_run_).Run();
}

UnwindScenario::UnwindScenario(const SetupFunction& setup_function)
    : setup_function_(setup_function) {}

UnwindScenario::~UnwindScenario() = default;

FunctionAddressRange UnwindScenario::GetWaitForSampleAddressRange() const {
  return WaitForSample(nullptr);
}

FunctionAddressRange UnwindScenario::GetSetupFunctionAddressRange() const {
  return setup_function_.Run(OnceClosure());
}

FunctionAddressRange UnwindScenario::GetOuterFunctionAddressRange() const {
  return InvokeSetupFunction(SetupFunction(), nullptr);
}

void UnwindScenario::Execute(SampleEvents* events) {
  InvokeSetupFunction(setup_function_, events);
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
UnwindScenario::InvokeSetupFunction(const SetupFunction& setup_function,
                                    SampleEvents* events) {
  const void* start_program_counter = GetProgramCounter();

  if (!setup_function.is_null()) {
    const auto wait_for_sample_closure =
        BindLambdaForTesting([&] { UnwindScenario::WaitForSample(events); });
    setup_function.Run(wait_for_sample_closure);
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// static
// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
UnwindScenario::WaitForSample(SampleEvents* events) {
  const void* start_program_counter = GetProgramCounter();

  if (events) {
    events->ready_for_sample.Signal();
    events->sample_finished.Wait();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
CallWithPlainFunction(OnceClosure wait_for_sample) {
  const void* start_program_counter = GetProgramCounter();

  if (!wait_for_sample.is_null())
    std::move(wait_for_sample).Run();

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange CallWithAlloca(OnceClosure wait_for_sample) {
  const void* start_program_counter = GetProgramCounter();

  // Volatile to force a dynamic stack allocation.
  const volatile size_t alloca_size = 100;
  // Use the memory via volatile writes to prevent the allocation from being
  // optimized out.
  volatile char* const allocation =
      const_cast<volatile char*>(static_cast<char*>(alloca(alloca_size)));
  for (volatile char* p = allocation; p < allocation + alloca_size; ++p)
    *p = '\0';

  if (!wait_for_sample.is_null())
    std::move(wait_for_sample).Run();

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

// Disable inlining for this function so that it gets its own stack frame.
NOINLINE FunctionAddressRange
CallThroughOtherLibrary(NativeLibrary library, OnceClosure wait_for_sample) {
  const void* start_program_counter = GetProgramCounter();

  if (!wait_for_sample.is_null()) {
    // A function whose arguments are a function accepting void*, and a void*.
    using InvokeCallbackFunction = void (*)(void (*)(void*), void*);
    EXPECT_TRUE(library);
    InvokeCallbackFunction function = reinterpret_cast<InvokeCallbackFunction>(
        GetFunctionPointerFromNativeLibrary(library, "InvokeCallbackFunction"));
    EXPECT_TRUE(function);
    (*function)(&OtherLibraryCallback, &wait_for_sample);
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

void WithTargetThread(UnwindScenario* scenario,
                      ProfileCallback profile_callback) {
  UnwindScenario::SampleEvents events;
  TargetThread target_thread(
      BindLambdaForTesting([&] { scenario->Execute(&events); }));

  target_thread.Start();
  events.ready_for_sample.Wait();

  std::move(profile_callback).Run(target_thread.thread_token());

  events.sample_finished.Signal();
  target_thread.Join();
}

std::vector<Frame> SampleScenario(UnwindScenario* scenario,
                                  ModuleCache* module_cache,
                                  UnwinderFactory aux_unwinder_factory) {
  StackSamplingProfiler::SamplingParams params;
  params.sampling_interval = Milliseconds(0);
  params.samples_per_profile = 1;

  std::vector<Frame> sample;
  WithTargetThread(
      scenario,
      BindLambdaForTesting(
          [&](SamplingProfilerThreadToken target_thread_token) {
            WaitableEvent sampling_thread_completed(
                WaitableEvent::ResetPolicy::MANUAL,
                WaitableEvent::InitialState::NOT_SIGNALED);
            StackSamplingProfiler profiler(
                target_thread_token, params,
                std::make_unique<TestProfileBuilder>(
                    module_cache,
                    BindLambdaForTesting([&sample, &sampling_thread_completed](
                                             std::vector<Frame> result_sample) {
                      sample = std::move(result_sample);
                      sampling_thread_completed.Signal();
                    })),
                CreateCoreUnwindersFactoryForTesting(module_cache));
            if (aux_unwinder_factory)
              profiler.AddAuxUnwinder(std::move(aux_unwinder_factory).Run());
            profiler.Start();
            sampling_thread_completed.Wait();
          }));

  return sample;
}

std::string FormatSampleForDiagnosticOutput(const std::vector<Frame>& sample) {
  std::string output;
  for (const auto& frame : sample) {
    output += StringPrintf(
        "0x%p %s\n", reinterpret_cast<const void*>(frame.instruction_pointer),
        frame.module ? frame.module->GetDebugBasename().AsUTF8Unsafe().c_str()
                     : "null module");
  }
  return output;
}

void ExpectStackContains(const std::vector<Frame>& stack,
                         const std::vector<FunctionAddressRange>& functions) {
  auto frame_it = stack.begin();
  auto function_it = functions.begin();
  for (; frame_it != stack.end() && function_it != functions.end();
       ++frame_it) {
    if (frame_it->instruction_pointer >=
            reinterpret_cast<uintptr_t>(function_it->start.get()) &&
        frame_it->instruction_pointer <=
            reinterpret_cast<uintptr_t>(function_it->end.get())) {
      ++function_it;
    }
  }

  EXPECT_EQ(function_it, functions.end())
      << "Function in position " << function_it - functions.begin() << " at "
      << function_it->start << " was not found in stack "
      << "(or did not appear in the expected order):\n"
      << FormatSampleForDiagnosticOutput(stack);
}

void ExpectStackDoesNotContain(
    const std::vector<Frame>& stack,
    const std::vector<FunctionAddressRange>& functions) {
  struct FunctionAddressRangeCompare {
    bool operator()(const FunctionAddressRange& a,
                    const FunctionAddressRange& b) const {
      return std::make_pair(a.start, a.end) < std::make_pair(b.start, b.end);
    }
  };

  std::set<FunctionAddressRange, FunctionAddressRangeCompare> seen_functions;
  for (const auto& frame : stack) {
    for (const auto& function : functions) {
      if (frame.instruction_pointer >=
              reinterpret_cast<uintptr_t>(function.start.get()) &&
          frame.instruction_pointer <=
              reinterpret_cast<uintptr_t>(function.end.get())) {
        seen_functions.insert(function);
      }
    }
  }

  for (const auto& function : seen_functions) {
    ADD_FAILURE() << "Function at " << function.start
                  << " was unexpectedly found in stack:\n"
                  << FormatSampleForDiagnosticOutput(stack);
  }
}

NativeLibrary LoadTestLibrary(std::string_view library_name) {
  // The lambda gymnastics works around the fact that we can't use ASSERT_*
  // macros in a function returning non-null.
  const auto load = [&](NativeLibrary* library) {
    FilePath library_path;
#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_IOS)
    // TODO(crbug.com/40799492): Find a solution that works across platforms.
    ASSERT_TRUE(PathService::Get(DIR_ASSETS, &library_path));
#else
    // The module is next to the test module rather than with test data.
    ASSERT_TRUE(PathService::Get(DIR_MODULE, &library_path));
#endif  // BUILDFLAG(IS_FUCHSIA)
    library_path =
        library_path.AppendASCII(GetLoadableModuleName(library_name));
    NativeLibraryLoadError load_error;
    *library = LoadNativeLibrary(library_path, &load_error);
    ASSERT_TRUE(*library) << "error loading " << library_path.value() << ": "
                          << load_error.ToString();
  };

  NativeLibrary library = nullptr;
  load(&library);
  return library;
}

NativeLibrary LoadOtherLibrary() {
  return LoadTestLibrary("base_profiler_test_support_library");
}

uintptr_t GetAddressInOtherLibrary(NativeLibrary library) {
  EXPECT_TRUE(library);
  uintptr_t address = reinterpret_cast<uintptr_t>(
      GetFunctionPointerFromNativeLibrary(library, "InvokeCallbackFunction"));
  EXPECT_NE(address, 0u);
  return address;
}

StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactoryForTesting(
    ModuleCache* module_cache) {
#if BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_ARM_CFI_TABLE)
  std::vector<std::unique_ptr<Unwinder>> unwinders;
  unwinders.push_back(CreateNativeUnwinderAndroidForTesting(
      reinterpret_cast<uintptr_t>(&__executable_start)));
  unwinders.push_back(CreateChromeUnwinderAndroidForTesting(
      reinterpret_cast<uintptr_t>(&__executable_start)));
  return BindOnce(
      [](std::vector<std::unique_ptr<Unwinder>> unwinders) {
        return unwinders;
      },
      std::move(unwinders));
#else
  return StackSamplingProfiler::UnwindersFactory();
#endif
}

uintptr_t TestModule::GetBaseAddress() const {
  return base_address_;
}
std::string TestModule::GetId() const {
  return id_;
}
FilePath TestModule::GetDebugBasename() const {
  return debug_basename_;
}
size_t TestModule::GetSize() const {
  return size_;
}
bool TestModule::IsNative() const {
  return is_native_;
}

bool operator==(const Frame& a, const Frame& b) {
  return a.instruction_pointer == b.instruction_pointer && a.module == b.module;
}

}  // namespace base
