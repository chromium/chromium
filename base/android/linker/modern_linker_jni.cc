// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Uses android_dlopen_ext() to share relocations.
//
// See linker_jni.h for more details and the dependency rules.

#include "base/android/linker/modern_linker_jni.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <jni.h>
#include <limits.h>
#include <link.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include <limits>
#include <memory>

#include <android/dlext.h>
#include "base/android/linker/linker_jni.h"

extern "C" {
// <android/dlext.h> does not declare android_dlopen_ext() if __ANDROID_API__
// is smaller than 21, so declare it here as a weak function. This will allow
// detecting its availability at runtime. For API level 21 or higher, the
// attribute is ignored due to the previous declaration.
void* android_dlopen_ext(const char*, int, const android_dlextinfo*)
    __attribute__((weak_import));

// This function is exported by the dynamic linker but never declared in any
// official header for some architecture/version combinations.
int dl_iterate_phdr(int (*cb)(dl_phdr_info* info, size_t size, void* data),
                    void* data) __attribute__((weak_import));
}  // extern "C"

namespace chromium_android_linker {
namespace {

// Record of the Java VM passed to JNI_OnLoad().
static JavaVM* s_java_vm = nullptr;

// Guarded by |sLock| in Linker.java.
RelroSharingStatus s_relro_sharing_status = RelroSharingStatus::NOT_ATTEMPTED;

}  // namespace

// Starting with API level 26 (Android O) the following functions from
// libandroid.so should be used to create shared memory regions to ensure
// compatibility with the future versions:
// * ASharedMemory_create()
// * ASharedMemory_setProt()
//
// This is inspired by //third_party/ashmem/ashmem-dev.c, which cannot be
// referenced from the linker library to avoid increasing binary size. Also
// there is no need to support API level <26 for ModernLinker.
//
// *Not* threadsafe.
struct SharedMemoryFunctions {
  SharedMemoryFunctions() {
    library_handle = dlopen("libandroid.so", RTLD_NOW);
    create = reinterpret_cast<CreateFunction>(
        dlsym(library_handle, "ASharedMemory_create"));
    set_protection = reinterpret_cast<SetProtectionFunction>(
        dlsym(library_handle, "ASharedMemory_setProt"));
  }

  bool IsWorking() const {
    if (!create || !set_protection) {
      LOG_ERROR("Cannot get the shared memory functions from libandroid");
      return false;
    }
    return true;
  }

  ~SharedMemoryFunctions() {
    if (library_handle)
      dlclose(library_handle);
  }

  typedef int (*CreateFunction)(const char*, size_t);
  typedef int (*SetProtectionFunction)(int fd, int prot);

  CreateFunction create;
  SetProtectionFunction set_protection;

  void* library_handle = nullptr;
};

namespace {

// Invokes android_dlopen_ext() to load the library into a given address range.
// Assumes that the address range is already reserved with mmap(2). On success,
// the |handle| of the loaded library is returned.
//
// Returns true iff this operation succeeds.
bool AndroidDlopenExt(void* mapping_start,
                      size_t mapping_size,
                      const char* filename,
                      void** handle) {
  if (!android_dlopen_ext) {
    LOG_ERROR("android_dlopen_ext is not found");
    return false;
  }

  android_dlextinfo dlextinfo{};
  dlextinfo.flags = ANDROID_DLEXT_RESERVED_ADDRESS;
  dlextinfo.reserved_addr = mapping_start;
  dlextinfo.reserved_size = mapping_size;

  LOG_INFO(
      "android_dlopen_ext:"
      " flags=0x%" PRIx64 ", reserved_addr=%p, reserved_size=%zu",
      dlextinfo.flags, dlextinfo.reserved_addr, dlextinfo.reserved_size);

  void* rv = android_dlopen_ext(filename, RTLD_NOW, &dlextinfo);
  if (rv == nullptr) {
    LOG_ERROR("android_dlopen_ext: %s", dlerror());
    return false;
  }

  *handle = rv;
  return true;
}

// With munmap(2) unmaps the tail of the given contiguous range of virtual
// memory. Ignores errors.
void TrimMapping(uintptr_t address, size_t old_size, size_t new_size) {
  if (old_size <= new_size) {
    LOG_ERROR("WARNING: library reservation was too small");
  } else {
    // Unmap the part of the reserved address space that is beyond the end of
    // the loaded library data.
    const uintptr_t unmap = address + new_size;
    const size_t length = old_size - new_size;
    munmap(reinterpret_cast<void*>(unmap), length);
  }
}

// Calls JNI_OnLoad() in the library referenced by |handle|.
// Returns true for success.
bool CallJniOnLoad(void* handle) {
  LOG_INFO("Entering");
  // Locate and if found then call the loaded library's JNI_OnLoad() function.
  using JNI_OnLoadFunctionPtr = int (*)(void* vm, void* reserved);
  auto jni_onload =
      reinterpret_cast<JNI_OnLoadFunctionPtr>(dlsym(handle, "JNI_OnLoad"));
  if (jni_onload != nullptr) {
    // Check that JNI_OnLoad returns a usable JNI version.
    int jni_version = (*jni_onload)(s_java_vm, nullptr);
    if (jni_version < JNI_VERSION_1_4) {
      LOG_ERROR("JNI version is invalid: %d", jni_version);
      return false;
    }
  }

  LOG_INFO("Done");
  return true;
}

}  // namespace

void NativeLibInfo::ExportLoadInfoToJava() const {
  if (!env_)
    return;
  s_lib_info_fields.SetLoadInfo(env_, java_object_, load_address_, load_size_);
}

void NativeLibInfo::ExportRelroInfoToJava() const {
  if (!env_)
    return;
  s_lib_info_fields.SetRelroInfo(env_, java_object_, relro_start_, relro_size_,
                                 relro_fd_);
}

void NativeLibInfo::CloseRelroFd() {
  if (relro_fd_ == kInvalidFd)
    return;
  close(relro_fd_);
  relro_fd_ = kInvalidFd;
}

// static
int NativeLibInfo::VisitLibraryPhdrs(dl_phdr_info* info,
                                     size_t size UNUSED,
                                     void* lib_info) {
  auto* out_lib_info = reinterpret_cast<NativeLibInfo*>(lib_info);
  ElfW(Addr) lookup_address =
      static_cast<ElfW(Addr)>(out_lib_info->load_address());

  // Use max and min vaddr to compute the library's load size.
  auto min_vaddr = std::numeric_limits<ElfW(Addr)>::max();
  ElfW(Addr) max_vaddr = 0;
  ElfW(Addr) min_relro_vaddr = ~0;
  ElfW(Addr) max_relro_vaddr = 0;

  bool is_matching = false;
  for (int i = 0; i < info->dlpi_phnum; ++i) {
    const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
    switch (phdr->p_type) {
      case PT_LOAD:
        // See if this segment's load address matches the value passed to
        // android_dlopen_ext as |extinfo.reserved_addr|.
        //
        // Here and below, the virtual address in memory is computed by
        //     address == info->dlpi_addr + program_header->p_vaddr
        // that is, the p_vaddr fields is relative to the object base address.
        // See dl_iterate_phdr(3) for details.
        if (lookup_address == info->dlpi_addr + phdr->p_vaddr)
          is_matching = true;

        if (phdr->p_vaddr < min_vaddr)
          min_vaddr = phdr->p_vaddr;
        if (phdr->p_vaddr + phdr->p_memsz > max_vaddr)
          max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        break;
      case PT_GNU_RELRO:
        min_relro_vaddr = PAGE_START(phdr->p_vaddr);
        max_relro_vaddr = phdr->p_vaddr + phdr->p_memsz;

        // As of 2020-11 in libmonochrome.so RELRO is covered by a LOAD segment.
        // It is not clear whether this property is going to be guaranteed in
        // the future. Include the RELRO segment as part of the 'load size'.
        // This way a potential future change in layout of LOAD segments would
        // not open address space for racy mmap(MAP_FIXED).
        if (min_relro_vaddr < min_vaddr)
          min_vaddr = min_relro_vaddr;
        if (max_vaddr < max_relro_vaddr)
          max_vaddr = max_relro_vaddr;
        break;
      default:
        break;
    }
  }

  // Fill out size and relro information if there was a match.
  if (is_matching) {
    int page_size = sysconf(_SC_PAGESIZE);
    if (page_size != PAGE_SIZE)
      abort();

    out_lib_info->load_size_ = PAGE_END(max_vaddr) - PAGE_START(min_vaddr);
    out_lib_info->relro_size_ =
        PAGE_END(max_relro_vaddr) - PAGE_START(min_relro_vaddr);
    out_lib_info->relro_start_ = info->dlpi_addr + PAGE_START(min_relro_vaddr);

    return true;
  }

  return false;
}

bool NativeLibInfo::FindRelroAndLibraryRangesInElf() {
  LOG_INFO("Called for 0x%" PRIxPTR, load_address_);
  if (!dl_iterate_phdr) {
    LOG_ERROR("No dl_iterate_phdr() found");
    return false;
  }
  int status = dl_iterate_phdr(&VisitLibraryPhdrs, this);
  if (!status) {
    LOG_ERROR("Failed to find library at address 0x%" PRIxPTR, load_address_);
    return false;
  }
  return true;
}

bool NativeLibInfo::LoadWithDlopenExt(const String& path, void** handle) {
  LOG_INFO("Entering");

  // The address range must be reserved during initialization in Linker.java.
  if (!load_address_) {
    // TODO(pasko): measure how often this happens.
    return false;
  }

  // Remember the memory reservation size. Starting from this point load_size_
  // changes the meaning to reflect the size of the loaded library.
  size_t reservation_size = load_size_;
  auto* address = reinterpret_cast<void*>(load_address_);

  // Invoke android_dlopen_ext.
  void* local_handle = nullptr;
  if (!AndroidDlopenExt(address, reservation_size, path.c_str(),
                        &local_handle)) {
    LOG_ERROR("android_dlopen_ext() error");
    munmap(address, load_size_);
    return false;
  }

  // Determine the library address ranges and the RELRO region.
  if (!FindRelroAndLibraryRangesInElf()) {
    // Fail early if PT_GNU_RELRO is not found. It likely indicates a
    // build misconfiguration.
    LOG_ERROR("Could not find RELRO in the loaded library: %s", path.c_str());
    abort();
    return false;
  }

  // Release the unused parts of the memory reservation.
  TrimMapping(load_address_, reservation_size, load_size_);

  *handle = local_handle;
  return true;
}

bool NativeLibInfo::CreateSharedRelroFd(
    const SharedMemoryFunctions& functions) {
  LOG_INFO("Entering");
  if (!relro_start_ || !relro_size_) {
    LOG_ERROR("RELRO region is not populated");
    return false;
  }

  // Create a writable shared memory region.
  int shared_mem_fd = functions.create("cr_relro", relro_size_);
  if (shared_mem_fd == -1) {
    LOG_ERROR("Cannot create the shared memory file");
    return false;
  }
  int rw_flags = PROT_READ | PROT_WRITE;
  functions.set_protection(shared_mem_fd, rw_flags);

  // Map the region as writable.
  void* relro_copy_addr =
      mmap(nullptr, relro_size_, rw_flags, MAP_SHARED, shared_mem_fd, 0);
  if (relro_copy_addr == MAP_FAILED) {
    PLOG_ERROR("failed to allocate space for copying RELRO");
    close(shared_mem_fd);
    return false;
  }

  // Populate the shared memory region with the contents of RELRO.
  void* relro_addr = reinterpret_cast<void*>(relro_start_);
  memcpy(relro_copy_addr, relro_addr, relro_size_);

  // Protect the underlying physical pages from further modifications from all
  // processes including the forked ones.
  //
  // Setting protection flags on the region to read-only guarantees that the
  // memory can no longer get mapped as writable, for any FD pointing to the
  // region, for any process. It is necessary to also munmap(2) the existing
  // writable memory mappings, since they are not directly affected by the
  // change of region's protection flags.
  munmap(relro_copy_addr, relro_size_);
  if (functions.set_protection(shared_mem_fd, PROT_READ) == -1) {
    LOG_ERROR("Failed to set the RELRO FD as read-only.");
    close(shared_mem_fd);
    return false;
  }

  relro_fd_ = shared_mem_fd;
  return true;
}

bool NativeLibInfo::ReplaceRelroWithSharedOne(
    const SharedMemoryFunctions& functions) const {
  LOG_INFO("Entering");
  if (relro_fd_ == -1 || !relro_start_ || !relro_size_) {
    LOG_ERROR("Replacement RELRO not ready");
    return false;
  }

  // Map as read-only to *atomically* replace the RELRO region provided by the
  // dynamic linker. To avoid memory corruption it is important that the
  // contents of both memory regions is identical.
  void* new_addr = mmap(reinterpret_cast<void*>(relro_start_), relro_size_,
                        PROT_READ, MAP_FIXED | MAP_SHARED, relro_fd_, 0);
  if (new_addr == MAP_FAILED) {
    PLOG_ERROR("mmap: replace RELRO");
    return false;
  }

  LOG_INFO("Replaced RELRO at 0x%" PRIxPTR, relro_start_);
  return true;
}

NativeLibInfo::NativeLibInfo(JNIEnv* env, jobject java_object)
    : env_(env), java_object_(java_object) {}

bool NativeLibInfo::CopyFromJavaObject() {
  if (!env_)
    return false;

  if (!s_lib_info_fields.GetLoadInfo(env_, java_object_, &load_address_,
                                     &load_size_)) {
    return false;
  }
  s_lib_info_fields.GetRelroInfo(env_, java_object_, &relro_start_,
                                 &relro_size_, &relro_fd_);
  return true;
}

bool NativeLibInfo::LoadLibrary(const String& library_path,
                                bool spawn_relro_region) {
  // Load the library.
  void* handle = nullptr;
  if (!LoadWithDlopenExt(library_path, &handle)) {
    LOG_ERROR("Failed to load native library: %s", library_path.c_str());
    return false;
  }
  if (!CallJniOnLoad(handle))
    return false;

  // Publish the library size and load address back to LibInfo in Java.
  ExportLoadInfoToJava();

  if (!spawn_relro_region)
    return true;

  // Spawn RELRO to a shared memory region by copying and remapping on top of
  // itself.
  SharedMemoryFunctions functions;
  if (!functions.IsWorking())
    return false;
  if (!CreateSharedRelroFd(functions)) {
    LOG_ERROR("Failed to create shared RELRO");
    return false;
  }
  if (!ReplaceRelroWithSharedOne(functions)) {
    LOG_ERROR("Failed to convert RELRO to shared memory");
    CloseRelroFd();
    return false;
  }

  LOG_INFO(
      "Created and converted RELRO to shared memory: relro_fd=%d, "
      "relro_start=0x%" PRIxPTR,
      relro_fd_, relro_start_);
  ExportRelroInfoToJava();
  return true;
}

bool NativeLibInfo::RelroIsIdentical(
    const NativeLibInfo& other_lib_info,
    const SharedMemoryFunctions& functions) const {
  // Abandon sharing if contents of the incoming RELRO region does not match the
  // current one. This can be useful for debugging, but should never happen in
  // the field.
  if (other_lib_info.relro_start_ != relro_start_ ||
      other_lib_info.relro_size_ != relro_size_ ||
      other_lib_info.load_size_ != load_size_) {
    LOG_ERROR("Incoming RELRO size does not match RELRO of the loaded library");
    return false;
  }
  void* shared_relro_address =
      mmap(nullptr, other_lib_info.relro_size_, PROT_READ, MAP_SHARED,
           other_lib_info.relro_fd_, 0);
  if (shared_relro_address == MAP_FAILED) {
    PLOG_ERROR("mmap: check RELRO is identical");
    return false;
  }
  void* current_relro_address = reinterpret_cast<void*>(relro_start_);
  int not_equal =
      memcmp(shared_relro_address, current_relro_address, relro_size_);
  munmap(shared_relro_address, relro_size_);
  if (not_equal) {
    LOG_ERROR("Relocations are not identical, giving up.");
    return false;
  }
  return true;
}

bool NativeLibInfo::CompareRelroAndReplaceItBy(
    const NativeLibInfo& other_lib_info) {
  if (other_lib_info.relro_fd_ == -1) {
    LOG_ERROR("No shared region to use");
    return false;
  }

  if (!FindRelroAndLibraryRangesInElf()) {
    LOG_ERROR("Could not find RELRO from externally provided address: 0x%p",
              reinterpret_cast<void*>(other_lib_info.load_address_));
    return false;
  }

  SharedMemoryFunctions functions;
  if (!functions.IsWorking())
    return false;
  if (!RelroIsIdentical(other_lib_info, functions)) {
    LOG_ERROR("RELRO is not identical");
    s_relro_sharing_status = RelroSharingStatus::NOT_IDENTICAL;
    return false;
  }

  // Make it shared.
  //
  // The alternative approach to invoke mprotect+mremap is probably faster than
  // munmap+mmap here. The advantage of the latter is that it removes all
  // formerly writable mappings, so:
  //  * It does not rely on disallowing mprotect(PROT_WRITE)
  //  * This way |ReplaceRelroWithSharedOne()| is reused across spawning RELRO
  //    and receiving it
  if (!other_lib_info.ReplaceRelroWithSharedOne(functions)) {
    LOG_ERROR("Failed to use relro_fd");
    // TODO(pasko): Introduce RelroSharingStatus::OTHER for rare RELRO sharing
    // failures like this one.
    return false;
  }

  s_relro_sharing_status = RelroSharingStatus::SHARED;
  return true;
}

bool NativeLibInfo::CreateSharedRelroFdForTesting() {
  // The library providing these functions will be dlclose()-ed after returning
  // from this context. The extra overhead of dlopen() is OK for testing.
  SharedMemoryFunctions functions;
  if (!functions.IsWorking())
    abort();
  return CreateSharedRelroFd(functions);
}

// static
bool NativeLibInfo::SharedMemoryFunctionsSupportedForTesting() {
  SharedMemoryFunctions functions;
  return functions.IsWorking();
}

JNI_GENERATOR_EXPORT jboolean
Java_org_chromium_base_library_1loader_ModernLinker_nativeLoadLibrary(
    JNIEnv* env,
    jclass clazz,
    jstring jdlopen_ext_path,
    jobject lib_info_obj,
    jboolean spawn_relro_region) {
  LOG_INFO("Entering");

  // Copy the contents from the Java-side LibInfo object.
  NativeLibInfo lib_info = {env, lib_info_obj};
  if (!lib_info.CopyFromJavaObject())
    return false;

  String library_path(env, jdlopen_ext_path);
  if (!lib_info.LoadLibrary(library_path, spawn_relro_region)) {
    return false;
  }
  return true;
}

JNI_GENERATOR_EXPORT jboolean
Java_org_chromium_base_library_1loader_ModernLinker_nativeUseRelros(
    JNIEnv* env,
    jclass clazz,
    jobject lib_info_obj) {
  LOG_INFO("Entering");
  // Copy the contents from the Java-side LibInfo object.
  NativeLibInfo incoming_lib_info = {env, lib_info_obj};
  if (!incoming_lib_info.CopyFromJavaObject())
    return false;

  // Create an empty NativeLibInfo to extract the current information about the
  // loaded library and later compare with the contents of the
  // |incoming_lib_info|.
  NativeLibInfo lib_info = {nullptr, nullptr};
  lib_info.set_load_address(incoming_lib_info.load_address());

  if (!lib_info.CompareRelroAndReplaceItBy(incoming_lib_info)) {
    return false;
  }
  return true;
}

JNI_GENERATOR_EXPORT jint
Java_org_chromium_base_library_1loader_ModernLinker_nativeGetRelroSharingResult(
    JNIEnv* env,
    jclass clazz) {
  return static_cast<jint>(s_relro_sharing_status);
}

bool ModernLinkerJNIInit(JavaVM* vm, JNIEnv* env) {
  s_java_vm = vm;
  return true;
}

}  // namespace chromium_android_linker
