// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/native_unwinder_android.h"

#include <sys/mman.h>

#include <string>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/native_unwinder_android_map_delegate.h"
#include "base/profiler/native_unwinder_android_memory_regions_map_impl.h"
#include "base/profiler/profile_builder.h"
#include "build/build_config.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Elf.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Maps.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Memory.h"
#include "third_party/libunwindstack/src/libunwindstack/include/unwindstack/Regs.h"

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

  std::string GetId() const override { return ""; }

  FilePath GetDebugBasename() const override {
    return FilePath(map_info_name_);
  }

  // Gets the size of the module.
  size_t GetSize() const override { return size_; }

  // True if this is a native module.
  bool IsNative() const override { return true; }

 private:
  const uintptr_t start_;
  const size_t size_;
  const std::string map_info_name_;
};

std::unique_ptr<unwindstack::Regs> CreateFromRegisterContext(
    RegisterContext* thread_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  return WrapUnique<unwindstack::Regs>(unwindstack::RegsArm::Read(
      reinterpret_cast<void*>(&thread_context->arm_r0)));
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
  return WrapUnique<unwindstack::Regs>(unwindstack::RegsArm64::Read(
      reinterpret_cast<void*>(&thread_context->regs[0])));
#else   // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  NOTREACHED();
#endif  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
}

void CopyToRegisterContext(unwindstack::Regs* regs,
                           RegisterContext* thread_context) {
#if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  memcpy(reinterpret_cast<void*>(&thread_context->arm_r0), regs->RawData(),
         unwindstack::ARM_REG_LAST * sizeof(uintptr_t));
#elif defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_64_BITS)
  memcpy(reinterpret_cast<void*>(&thread_context->regs[0]), regs->RawData(),
         unwindstack::ARM64_REG_LAST * sizeof(uintptr_t));
#else   // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
  NOTREACHED();
#endif  // #if defined(ARCH_CPU_ARM_FAMILY) && defined(ARCH_CPU_32_BITS)
}

}  // namespace

UnwindStackMemoryAndroid::UnwindStackMemoryAndroid(uintptr_t stack_ptr,
                                                   uintptr_t stack_top)
    : stack_ptr_(stack_ptr), stack_top_(stack_top) {
  DCHECK_LE(stack_ptr_, stack_top_);
}

UnwindStackMemoryAndroid::~UnwindStackMemoryAndroid() = default;

size_t UnwindStackMemoryAndroid::Read(uint64_t addr, void* dst, size_t size) {
  if (addr < stack_ptr_)
    return 0;
  if (size >= stack_top_ || addr > stack_top_ - size)
    return 0;
  memcpy(dst, reinterpret_cast<void*>(addr), size);
  return size;
}

// static
std::unique_ptr<NativeUnwinderAndroidMemoryRegionsMap>
NativeUnwinderAndroid::CreateMemoryRegionsMap(bool use_updatable_maps) {
  std::unique_ptr<unwindstack::Maps> maps;
  if (use_updatable_maps) {
    maps = std::make_unique<unwindstack::LocalUpdatableMaps>();
  } else {
    maps = std::make_unique<unwindstack::LocalMaps>();
  }
  const bool success = maps->Parse();
  DCHECK(success);

  return std::make_unique<NativeUnwinderAndroidMemoryRegionsMapImpl>(
      std::move(maps), unwindstack::Memory::CreateLocalProcessMemory());
}

NativeUnwinderAndroid::NativeUnwinderAndroid(
    uintptr_t exclude_module_with_base_address,
    NativeUnwinderAndroidMapDelegate* map_delegate)
    : exclude_module_with_base_address_(exclude_module_with_base_address),
      map_delegate_(map_delegate),
      memory_regions_map_(
          static_cast<NativeUnwinderAndroidMemoryRegionsMapImpl*>(
              map_delegate->GetMapReference())) {
  DCHECK(map_delegate_);
  DCHECK(memory_regions_map_);
}

NativeUnwinderAndroid::~NativeUnwinderAndroid() {
  if (module_cache())
    module_cache()->UnregisterAuxiliaryModuleProvider(this);

  map_delegate_->ReleaseMapReference();
}

void NativeUnwinderAndroid::InitializeModules() {
  module_cache()->RegisterAuxiliaryModuleProvider(this);
}

bool NativeUnwinderAndroid::CanUnwindFrom(const Frame& current_frame) const {
  return current_frame.module && current_frame.module->IsNative() &&
         current_frame.module->GetBaseAddress() !=
             exclude_module_with_base_address_;
}

UnwindResult NativeUnwinderAndroid::TryUnwind(
    UnwinderStateCapture* capture_state,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<Frame>* stack) {
  auto regs = CreateFromRegisterContext(thread_context);
  DCHECK(regs);
  unwindstack::ArchEnum arch = regs->Arch();

  do {
    uint64_t cur_pc = regs->pc();
    uint64_t cur_sp = regs->sp();
    unwindstack::MapInfo* map_info =
        memory_regions_map_->maps()->Find(cur_pc).get();
    if (map_info == nullptr ||
        map_info->flags() & unwindstack::MAPS_FLAGS_DEVICE_MAP) {
      break;
    }

    unwindstack::Elf* elf =
        map_info->GetElf(memory_regions_map_->memory(), arch);
    if (!elf->valid())
      break;

    UnwindStackMemoryAndroid stack_memory(cur_sp, stack_top);
    uintptr_t rel_pc = elf->GetRelPc(cur_pc, map_info);
    bool is_signal_frame = false;
    bool finished = false;
    // map_info->GetElf() may return a valid elf whose memory() is nullptr.
    // In the case, elf->StepIfSignalHandler() and elf->Step() are not
    // available, because the method depends on elf->memory().
    // (Regarding Step(), EvalRegister() needs memory.)
    bool stepped =
        elf->memory() &&
        (elf->StepIfSignalHandler(rel_pc, regs.get(), &stack_memory) ||
         elf->Step(rel_pc, regs.get(), &stack_memory, &finished,
                   &is_signal_frame));
    if (stepped && finished)
      return UnwindResult::kCompleted;

    if (!stepped) {
      // Stepping failed. Try unwinding using return address.
      if (stack->size() == 1) {
        if (!regs->SetPcFromReturnAddress(&stack_memory))
          return UnwindResult::kAborted;
      } else {
        break;
      }
    }

    // If the pc and sp didn't change, then consider everything stopped.
    if (cur_pc == regs->pc() && cur_sp == regs->sp())
      return UnwindResult::kAborted;

    // Exclusive range of expected stack pointer values after the unwind.
    struct {
      uintptr_t start;
      uintptr_t end;
    } expected_stack_pointer_range = {static_cast<uintptr_t>(cur_sp),
                                      stack_top};
    if (regs->sp() < expected_stack_pointer_range.start ||
        regs->sp() >= expected_stack_pointer_range.end) {
      return UnwindResult::kAborted;
    }

    if (regs->dex_pc() != 0) {
      // Add a frame to represent the dex file.
      EmitDexFrame(regs->dex_pc(), arch, stack);

      // Clear the dex pc so that we don't repeat this frame later.
      regs->set_dex_pc(0);
    }

    // Add the frame to |stack|. Must use GetModuleForAddress rather than
    // GetExistingModuleForAddress because the unwound-to address may be in a
    // module associated with a different unwinder.
    const ModuleCache::Module* module =
        module_cache()->GetModuleForAddress(regs->pc());
    stack->emplace_back(regs->pc(), module);
  } while (CanUnwindFrom(stack->back()));

  // Restore registers necessary for further unwinding in |thread_context|.
  CopyToRegisterContext(regs.get(), thread_context);
  return UnwindResult::kUnrecognizedFrame;
}

std::unique_ptr<const ModuleCache::Module>
NativeUnwinderAndroid::TryCreateModuleForAddress(uintptr_t address) {
  unwindstack::MapInfo* map_info =
      memory_regions_map_->maps()->Find(address).get();
  if (map_info == nullptr || !(map_info->flags() & PROT_EXEC) ||
      map_info->flags() & unwindstack::MAPS_FLAGS_DEVICE_MAP) {
    return nullptr;
  }
  return std::make_unique<NonElfModule>(map_info);
}

unwindstack::DexFiles* NativeUnwinderAndroid::GetOrCreateDexFiles(
    unwindstack::ArchEnum arch) {
  if (!dex_files_) {
    dex_files_ = unwindstack::CreateDexFiles(
        arch, memory_regions_map_->memory(), search_libs_);
  }
  return dex_files_.get();
}

void NativeUnwinderAndroid::EmitDexFrame(uintptr_t dex_pc,
                                         unwindstack::ArchEnum arch,
                                         std::vector<Frame>* stack) {
  const ModuleCache::Module* module =
      module_cache()->GetExistingModuleForAddress(dex_pc);
  if (!module) {
    // The region containing |dex_pc| may not be in module_cache() since it's
    // usually not executable (.dex file). Since non-executable regions
    // are used much less commonly, it's lazily added here instead of from
    // AddInitialModulesFromMaps().
    unwindstack::MapInfo* map_info =
        memory_regions_map_->maps()->Find(dex_pc).get();
    if (map_info) {
      auto new_module = std::make_unique<NonElfModule>(map_info);
      module = new_module.get();
      module_cache()->AddCustomNativeModule(std::move(new_module));
    }
  }

    stack->emplace_back(dex_pc, module);
}

}  // namespace base
