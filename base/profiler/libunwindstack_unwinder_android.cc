// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/libunwindstack_unwinder_android.h"

#include <sys/mman.h>

#include <string>
#include <vector>

#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Elf.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Error.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Maps.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Memory.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Regs.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Unwinder.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
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
  return base::WrapUnique<unwindstack::Regs>(unwindstack::RegsArm::Read(
      reinterpret_cast<void*>(&thread_context->arm_r0)));
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
  return base::WrapUnique<unwindstack::Regs>(unwindstack::RegsArm64::Read(
      reinterpret_cast<void*>(&thread_context->regs[0])));
#else   // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  NOTREACHED();
#endif  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
}

void WriteLibunwindstackTraceEventArgs(unwindstack::ErrorCode error_code,
                                       std::optional<int> num_frames,
                                       perfetto::EventContext& ctx) {
  auto* track_event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
  auto* libunwindstack_unwinder = track_event->set_libunwindstack_unwinder();
  using ProtoEnum = perfetto::protos::pbzero::LibunwindstackUnwinder::ErrorCode;
  libunwindstack_unwinder->set_error_code(static_cast<ProtoEnum>(error_code));
  if (num_frames.has_value()) {
    libunwindstack_unwinder->set_num_frames(*num_frames);
  }
}

bool IsJavaModule(const base::ModuleCache::Module* module) {
  if (!module) {
    return false;
  }

  const auto path = module->GetDebugBasename();
  const std::string& debug_basename = path.value();

  return debug_basename.find("chrome.apk") != std::string::npos ||
         debug_basename.find("base.apk") != std::string::npos;
}

}  // namespace

LibunwindstackUnwinderAndroid::LibunwindstackUnwinderAndroid()
    : memory_regions_map_(
          static_cast<NativeUnwinderAndroidMemoryRegionsMapImpl*>(
              NativeUnwinderAndroid::CreateMemoryRegionsMap(
                  /*use_updatable_maps=*/false)
                  .release())) {
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
    jit_debug_ = unwindstack::CreateJitDebug(
        arch, memory_regions_map_->memory(), search_libs_);
  }
  return jit_debug_.get();
}

unwindstack::DexFiles* LibunwindstackUnwinderAndroid::GetOrCreateDexFiles(
    unwindstack::ArchEnum arch) {
  if (!dex_files_) {
    dex_files_ = unwindstack::CreateDexFiles(
        arch, memory_regions_map_->memory(), search_libs_);
  }
  return dex_files_.get();
}

UnwindResult LibunwindstackUnwinderAndroid::TryUnwind(
    UnwinderStateCapture* capture_state,
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

  std::unique_ptr<unwindstack::Regs> regs =
      CreateFromRegisterContext(thread_context);
  DCHECK(regs);

  TRACE_EVENT_BEGIN(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                    "libunwindstack::Unwind");
  unwindstack::Unwinder unwinder(kMaxFrames, memory_regions_map_->maps(),
                                 regs.get(), memory_regions_map_->memory());

  unwinder.SetJitDebug(GetOrCreateJitDebug(regs->Arch()));
  unwinder.SetDexFiles(GetOrCreateDexFiles(regs->Arch()));

  unwinder.Unwind(/*initial_map_names_to_skip=*/nullptr,
                  /*map_suffixes_to_ignore=*/nullptr);
  TRACE_EVENT_END(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"));

  // Currently libunwindstack doesn't support warnings.
  UnwindValues values =
      UnwindValues{unwinder.LastErrorCode(), /*unwinder.warnings()*/ 0,
                   unwinder.ConsumeFrames()};

  if (values.error_code != unwindstack::ERROR_NONE) {
    TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
                        "Libunwindstack Failure",
                        [&values](perfetto::EventContext& ctx) {
                          WriteLibunwindstackTraceEventArgs(
                              values.error_code, values.frames.size(), ctx);
                        });
  }
  if (values.frames.empty()) {
    return UnwindResult::kCompleted;
  }

  // The list of frames provided by Libunwindstack's Unwind() contains the
  // executing frame. The executing frame is also added by
  // StackSamplerImpl::WalkStack(). Ignore the frame from the latter to avoid
  // duplication. In case a java method was being interpreted libunwindstack
  // adds a dummy frame for it and then writes the corresponding native frame.
  // In such a scenario we want to prefer the frames produced by
  // libunwindstack.
  DCHECK_EQ(stack->size(), 1u);
  stack->clear();

  for (const unwindstack::FrameData& frame : values.frames) {
    const ModuleCache::Module* module =
        module_cache()->GetModuleForAddress(frame.pc);
    if (module == nullptr && frame.map_info != nullptr) {
      // Try searching for the module with same module start.
      module = module_cache()->GetModuleForAddress(frame.map_info->start());
      if (module == nullptr) {
        auto module_for_caching =
            std::make_unique<NonElfModule>(frame.map_info.get());
        module = module_for_caching.get();
        module_cache()->AddCustomNativeModule(std::move(module_for_caching));
      }
      if (frame.pc < frame.map_info->start() ||
          frame.pc >= frame.map_info->end()) {
        TRACE_EVENT_INSTANT(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                            "PC out of map range",
                            [&values](perfetto::EventContext& ctx) {
                              WriteLibunwindstackTraceEventArgs(
                                  values.error_code, std::nullopt, ctx);
                            });
      }
    }
    std::string function_name = IsJavaModule(module) ? frame.function_name : "";
    stack->emplace_back(frame.pc, module, std::move(function_name));
  }
  return UnwindResult::kCompleted;
}

}  // namespace base
