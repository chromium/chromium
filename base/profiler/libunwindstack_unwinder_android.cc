// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/libunwindstack_unwinder_android.h"

#include <sys/mman.h>

#include <string>
#include <vector>

#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Elf.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Memory.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Regs.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Unwinder.h"

#include "base/logging.h"
#include "base/notreached.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/native_unwinder_android.h"
#include "base/profiler/profile_builder.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/MachineArm.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/RegsArm.h"
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/MachineArm64.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/RegsArm64.h"
#endif  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)

namespace base {
namespace {
// How frequently we're willing to try and reparse maps. Sometimes, dynamic
// libraries get added and can cause unwinding to fail which can be solved by
// reparsing maps. However reparsing maps is an expensive operation and we don't
// want to cause churn if there is for some reason some map consistently
// failing.
//
// if we're sampling every 50ms, 1200 samples is 1 minute.
const int kMinSamplesBeforeNextMapsParse = 1200;

class NonElfModule : public ModuleCache::Module {
 public:
  explicit NonElfModule(unwindstack::MapInfo* map_info)
      : start_(map_info->start()),
        size_(map_info->end() - start_),
        map_info_name_(map_info->name()) {}
  ~NonElfModule() override = default;

  uintptr_t GetBaseAddress() const override { return start_; }

  std::string GetId() const override { return std::string(); }

  FilePath GetDebugBasename() const override {
    return FilePath(map_info_name_);
  }

  size_t GetSize() const override { return size_; }

  bool IsNative() const override { return true; }

 private:
  const uintptr_t start_;
  const size_t size_;
  const std::string map_info_name_;
};

std::unique_ptr<unwindstack::Regs> CreateFromRegisterContext(
    RegisterContext* thread_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  return absl::WrapUnique<unwindstack::Regs>(unwindstack::RegsArm::Read(
      reinterpret_cast<void*>(&thread_context->arm_r0)));
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
  return absl::WrapUnique<unwindstack::Regs>(unwindstack::RegsArm64::Read(
      reinterpret_cast<void*>(&thread_context->regs[0])));
#else   // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  NOTREACHED();
  return nullptr;
#endif  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
}
}  // namespace

LibunwindstackUnwinderAndroid::LibunwindstackUnwinderAndroid()
    : memory_regions_map_(NativeUnwinderAndroid::CreateMaps()),
      process_memory_(std::shared_ptr<unwindstack::Memory>(
          NativeUnwinderAndroid::CreateProcessMemory().release())) {
  TRACE_EVENT_INSTANT(
      TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
      "LibunwindstackUnwinderAndroid::LibunwindstackUnwinderAndroid");
}

LibunwindstackUnwinderAndroid::~LibunwindstackUnwinderAndroid() = default;

void LibunwindstackUnwinderAndroid::InitializeModules() {}

bool LibunwindstackUnwinderAndroid::CanUnwindFrom(
    const Frame& current_frame) const {
  return true;
}

unwindstack::JitDebug* LibunwindstackUnwinderAndroid::GetOrCreateJitDebug(
    unwindstack::ArchEnum arch) {
  if (!jit_debug_) {
    jit_debug_ =
        unwindstack::CreateJitDebug(arch, process_memory_, search_libs_);
  }
  return jit_debug_.get();
}

unwindstack::DexFiles* LibunwindstackUnwinderAndroid::GetOrCreateDexFiles(
    unwindstack::ArchEnum arch) {
  if (!dex_files_) {
    dex_files_ =
        unwindstack::CreateDexFiles(arch, process_memory_, search_libs_);
  }
  return dex_files_.get();
}

UnwindResult LibunwindstackUnwinderAndroid::TryUnwind(
    RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<Frame>* stack) {
  TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
              "LibunwindstackUnwinderAndroid::TryUnwind");
  // 500 is taken from traced_perf's own limit:
  // https://cs.android.com/android/platform/superproject/+/master:external/perfetto/src/profiling/memory/unwinding.cc;l=64;drc=5860970a8606bb48059aa31ee506328286b9bf92
  const int kMaxFrames = 500;

  // We use a struct and lambda here to cleanly express the result of an attempt
  // to unwind. Sometimes when we fail we can succeed if we reparse maps and so
  // we will call |attempt_unwind| twice.
  struct UnwindValues {
    unwindstack::ErrorCode error_code;
    uint64_t warnings;
    std::vector<unwindstack::FrameData> frames;
  };

  auto attempt_unwind = [&]() {
    // regs will get clobbered by each attempt, so if it fails we have to
    // start fresh from the initial context.
    std::unique_ptr<unwindstack::Regs> regs =
        CreateFromRegisterContext(thread_context);
    DCHECK(regs);
    unwindstack::Unwinder unwinder(kMaxFrames, memory_regions_map_.get(),
                                   regs.get(), process_memory_);

    unwinder.SetJitDebug(GetOrCreateJitDebug(regs->Arch()));
    unwinder.SetDexFiles(GetOrCreateDexFiles(regs->Arch()));

    unwinder.Unwind(/*initial_map_names_to_skip=*/nullptr,
                    /*map_suffixes_to_ignore=*/nullptr);
    ++samples_since_last_maps_parse_;
    // Currently libunwindstack doesn't support warnings.
    return UnwindValues{unwinder.LastErrorCode(), /*unwinder.warnings()*/ 0,
                        unwinder.ConsumeFrames()};
  };

  // We now proceed with the first unwind.
  UnwindValues values = attempt_unwind();

  // If our maps are invalid and we haven't reparsed in awhile then attempt to
  // reparse the maps and reunwind the stack to recover from the error.
  bool should_retry =
      values.error_code == unwindstack::ERROR_INVALID_MAP &&
      samples_since_last_maps_parse_ > kMinSamplesBeforeNextMapsParse;
  if (should_retry) {
    TRACE_EVENT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                "TryUnwind Reparsing Maps");
    samples_since_last_maps_parse_ = 0;
    memory_regions_map_->Parse();
    jit_debug_.reset();
    dex_files_.reset();

    // Our second attempt will override our first attempt when we check the
    // result later.
    values = attempt_unwind();
  }

  // Check the result of either the first or second unwind. If we were
  // successful transfer from libunwindstack format into base::Unwinder format.
  if (values.error_code == unwindstack::ERROR_NONE) {
    // The list of frames provided by Libunwindstack's Unwind() contains the
    // executing frame. The executing frame is also added by
    // StackSamplerImpl::WalkStack(). Ignore the frame from the latter to avoid
    // duplication. In case a java method was being interpreted libunwindstack
    // adds a dummy frame for it and then writes the corresponding native frame.
    // In such a scenario we want to prefer the frames produced by
    // libunwindstack.
    DCHECK_EQ(stack->size(), 1u);
    // Since libunwindstack completed unwinding without errors, so the frames
    // list shouldn't be empty.
    DCHECK(!values.frames.empty());
    stack->clear();
    for (const unwindstack::FrameData& frame : values.frames) {
      const ModuleCache::Module* module =
          module_cache()->GetModuleForAddress(frame.pc);
      if (module == nullptr && frame.map_info != nullptr) {
        auto module_for_caching =
            std::make_unique<NonElfModule>(frame.map_info.get());
        module = module_for_caching.get();
        module_cache()->AddCustomNativeModule(std::move(module_for_caching));
      }
      stack->emplace_back(frame.pc, module, frame.function_name);
    }
    return UnwindResult::kCompleted;
  }
  TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                      "TryUnwind Failure", "error", values.error_code,
                      "warning", values.warnings, "num_frames",
                      values.frames.size());
  return UnwindResult::kAborted;
}
}  // namespace base
