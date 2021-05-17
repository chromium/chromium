// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/module_cache.h"

#include <dlfcn.h>
#include <elf.h>

#include "base/debug/elf_reader.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// arm64 has execute-only memory (XOM) protecting code pages from being read.
// PosixModule reads executable pages in order to extract module info. This may
// result in a crash if the module is mapped as XOM so the code is disabled on
// that arch. See https://crbug.com/957801.
#if defined(OS_ANDROID) && !defined(ARCH_CPU_ARM64)
extern "C" {
// &__executable_start is the start address of the current module.
extern const char __executable_start;
// &__etext is the end addesss of the code segment in the current module.
extern const char _etext;
}
#endif  // defined(OS_ANDROID) && !defined(ARCH_CPU_ARM64)

namespace base {

namespace {

#if !defined(ARCH_CPU_ARM64)
// Returns the unique build ID for a module loaded at |module_addr|. Returns the
// empty string if the function fails to get the build ID.
//
// Build IDs follow a cross-platform format consisting of several fields
// concatenated together:
// - the module's unique ID, and
// - the age suffix for incremental builds.
//
// On POSIX, the unique ID is read from the ELF binary located at |module_addr|.
// The age field is always 0.
std::string GetUniqueBuildId(const void* module_addr) {
  debug::ElfBuildIdBuffer build_id;
  size_t build_id_length = debug::ReadElfBuildId(module_addr, true, build_id);
  if (!build_id_length)
    return std::string();

  // Append 0 for the age value.
  return std::string(build_id, build_id_length) + "0";
}

// Returns the offset from |module_addr| to the first byte following the last
// executable segment from the ELF file mapped at |module_addr|.
// It's defined this way so that any executable address from this module is in
// range [addr, addr + GetLastExecutableOffset(addr)).
// If no executable segment is found, returns 0.
size_t GetLastExecutableOffset(const void* module_addr) {
  const size_t relocation_offset = debug::GetRelocationOffset(module_addr);
  size_t max_offset = 0;
  for (const Phdr& header : debug::GetElfProgramHeaders(module_addr)) {
    if (header.p_type != PT_LOAD || !(header.p_flags & PF_X))
      continue;

    max_offset = std::max(
        max_offset, static_cast<size_t>(
                        header.p_vaddr + relocation_offset + header.p_memsz -
                        reinterpret_cast<uintptr_t>(module_addr)));
  }

  return max_offset;
}

FilePath GetDebugBasenameForModule(const void* base_address, const char* file) {
#if defined(OS_ANDROID)
  // Preferentially identify the library using its soname on Android. Libraries
  // mapped directly from apks have the apk filename in |dl_info.dli_fname|, and
  // this doesn't distinguish the particular library.
  absl::optional<StringPiece> library_name =
      debug::ReadElfLibraryName(base_address);
  if (library_name)
    return FilePath(*library_name);
#endif  // defined(OS_ANDROID)

  return FilePath(file).BaseName();
}
#endif  // !defined(ARCH_CPU_ARM64)

class PosixModule : public ModuleCache::Module {
 public:
  PosixModule(uintptr_t base_address,
              const std::string& build_id,
              const FilePath& debug_basename,
              size_t size);

  PosixModule(const PosixModule&) = delete;
  PosixModule& operator=(const PosixModule&) = delete;

  // ModuleCache::Module
  uintptr_t GetBaseAddress() const override { return base_address_; }
  std::string GetId() const override { return id_; }
  FilePath GetDebugBasename() const override { return debug_basename_; }
  size_t GetSize() const override { return size_; }
  bool IsNative() const override { return true; }

 private:
  uintptr_t base_address_;
  std::string id_;
  FilePath debug_basename_;
  size_t size_;
};

PosixModule::PosixModule(uintptr_t base_address,
                         const std::string& build_id,
                         const FilePath& debug_basename,
                         size_t size)
    : base_address_(base_address),
      id_(build_id),
      debug_basename_(debug_basename),
      size_(size) {}

}  // namespace

// static
std::unique_ptr<const ModuleCache::Module> ModuleCache::CreateModuleForAddress(
    uintptr_t address) {
#if defined(ARCH_CPU_ARM64)
  // arm64 has execute-only memory (XOM) protecting code pages from being read.
  // PosixModule reads executable pages in order to extract module info. This
  // may result in a crash if the module is mapped as XOM
  // (https://crbug.com/957801).
  return nullptr;
#else
  Dl_info info;
  if (!dladdr(reinterpret_cast<const void*>(address), &info)) {
#if defined(OS_ANDROID)
    // dladdr doesn't know about the Chrome module in Android targets using the
    // crazy linker. Explicitly check against the module's extents in that case.
    if (address >= reinterpret_cast<uintptr_t>(&__executable_start) &&
        address < reinterpret_cast<uintptr_t>(&_etext)) {
      const void* const base_address =
          reinterpret_cast<const void*>(&__executable_start);
      return std::make_unique<PosixModule>(
          reinterpret_cast<uintptr_t>(&__executable_start),
          GetUniqueBuildId(base_address),
          // Extract the soname from the module. It is expected to exist, but if
          // it doesn't use an empty string.
          GetDebugBasenameForModule(base_address, /* file = */ ""),
          GetLastExecutableOffset(base_address));
    }
#endif
    return nullptr;
  }

  return std::make_unique<PosixModule>(
      reinterpret_cast<uintptr_t>(info.dli_fbase),
      GetUniqueBuildId(info.dli_fbase),
      GetDebugBasenameForModule(info.dli_fbase, info.dli_fname),
      GetLastExecutableOffset(info.dli_fbase));
#endif
}

}  // namespace base
