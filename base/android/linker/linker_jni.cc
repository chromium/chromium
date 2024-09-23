// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <android/dlext.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <jni.h>
#include <link.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/android/linker/linker_jni.h"

namespace chromium_android_linker {

namespace {

// Variable containing LibInfo for the loaded library.
LibInfo_class s_lib_info_fields;

// Guarded by |mLock| in Linker.java.
RelroSharingStatus s_relro_sharing_status = RelroSharingStatus::NOT_ATTEMPTED;

// Saved JavaVM passed to JNI_OnLoad().
JavaVM* s_java_vm = nullptr;

size_t GetPageSize() {
  return sysconf(_SC_PAGESIZE);
}

// With mmap(2) reserves a range of virtual addresses.
//
// The range must start with |hint| and be of size |size|. The |hint==0|
// indicates that the address of the mapping should be chosen at random,
// utilizing ASLR built into mmap(2).
//
// The start of the resulting region is returned in |address|.
//
// The value 0 returned iff the attempt failed (a part of the address range is
// already reserved by some other subsystem).
void ReserveAddressWithHint(uintptr_t hint, uintptr_t* address, size_t* size) {
  void* ptr = reinterpret_cast<void*>(hint);
  void* new_ptr = mmap(ptr, kAddressSpaceReservationSize, PROT_NONE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (new_ptr == MAP_FAILED) {
    PLOG_ERROR("mmap");
    *address = 0;
  } else if ((hint != 0) && (new_ptr != ptr)) {
    // Something grabbed the address range before the early phase of the
    // linker had a chance, this should be uncommon.
    LOG_ERROR("Address range starting at 0x%" PRIxPTR " was not free to use",
              hint);
    munmap(new_ptr, kAddressSpaceReservationSize);
    *address = 0;
  } else {
    *address = reinterpret_cast<uintptr_t>(new_ptr);
    *size = kAddressSpaceReservationSize;
    LOG_INFO("Reserved region at address: 0x%" PRIxPTR ", size: 0x%zu",
             *address, *size);
  }
}

bool ScanRegionInBuffer(const char* buf,
                        size_t length,
                        uintptr_t* out_address,
                        size_t* out_size) {
  const char* position = strstr(buf, "[anon:libwebview reservation]");
  if (!position)
    return false;

  const char* line_start = position;
  while (line_start > buf) {
    line_start--;
    if (*line_start == '\n') {
      line_start++;
      break;
    }
  }

  // Extract the region start and end. The failures below should not happen as
  // long as the reservation is made the same way in
  // frameworks/base/native/webview/loader/loader.cpp.
  uintptr_t vma_start, vma_end;
  char permissions[5] = {'\0'};  // Ensure a null-terminated string.
  // Example line from proc(5):
  // address           perms offset  dev   inode   pathname
  // 00400000-00452000 r-xp 00000000 08:02 173521  /usr/bin/dbus-daemon
  if (sscanf(line_start, "%" SCNxPTR "-%" SCNxPTR " %4c", &vma_start, &vma_end,
             permissions) < 3) {
    return false;
  }

  if (strcmp(permissions, "---p"))
    return false;

  const size_t kPageSize = GetPageSize();
  if (vma_start % kPageSize || vma_end % kPageSize) {
    return false;
  }

  *out_address = static_cast<uintptr_t>(vma_start);
  *out_size = vma_end - vma_start;

  return true;
}

bool FindRegionInOpenFile(int fd, uintptr_t* out_address, size_t* out_size) {
  constexpr size_t kMaxLineLength = 256;
  const size_t kPageSize = GetPageSize();
  const size_t kReadSize = kPageSize;

  // Loop until no bytes left to scan. On every iteration except the last, fill
  // the buffer till the end. On every iteration except the first, the buffer
  // begins with kMaxLineLength bytes from the end of the previous fill.

// Silence clang's warning about allocating on the stack because this is a very
// special case.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wvla-extension"
  char buf[kReadSize + kMaxLineLength + 1];
#pragma clang diagnostic pop

  buf[kReadSize + kMaxLineLength] = '\0';  // Stop strstr().
  size_t pos = 0;
  size_t bytes_requested = kReadSize + kMaxLineLength;
  bool reached_end = false;
  while (true) {
    // Fill the |buf| to the maximum and determine whether reading reached the
    // end.
    size_t bytes_read = 0;
    do {
      ssize_t rv = HANDLE_EINTR(
          read(fd, buf + pos + bytes_read, bytes_requested - bytes_read));
      if (rv == 0) {
        reached_end = true;
      } else if (rv < 0) {
        PLOG_ERROR("read to find webview reservation");
        return false;
      }
      bytes_read += rv;
    } while (!reached_end && (bytes_read < bytes_requested));

    // Return results if the buffer contains the pattern.
    if (ScanRegionInBuffer(buf, pos + bytes_read, out_address, out_size))
      return true;

    // Did not find the pattern.
    if (reached_end)
      return false;

    // The buffer is filled to the end. Copy the end bytes to the beginning,
    // allowing to scan these bytes on the next iteration.
    memcpy(buf, buf + kReadSize, kMaxLineLength);
    pos = kMaxLineLength;
    bytes_requested = kReadSize;
  }
}

// Invokes android_dlopen_ext() to load the library into a given address range.
// Assumes that the address range is already reserved with mmap(2). On success,
// the |handle| of the loaded library is returned.
//
// Returns true iff this operation succeeds.
bool AndroidDlopenExt(void* mapping_start,
                      size_t mapping_size,
                      const char* filename,
                      void** handle) {
  android_dlextinfo dlextinfo = {};
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

String::String(JNIEnv* env, jstring str) {
  size_ = env->GetStringUTFLength(str);
  ptr_ = static_cast<char*>(::malloc(size_ + 1));

  // Note: This runs before browser native code is loaded, and so cannot
  // rely on anything from base/. This means that we must use
  // GetStringUTFChars() and not base::android::ConvertJavaStringToUTF8().
  //
  // GetStringUTFChars() suffices because the only strings used here are
  // paths to APK files or names of shared libraries, all of which are
  // plain ASCII, defined and hard-coded by the Chromium Android build.
  //
  // For more: see
  //   https://crbug.com/508876
  //
  // Note: GetStringUTFChars() returns Java UTF-8 bytes. This is good
  // enough for the linker though.
  const char* bytes = env->GetStringUTFChars(str, nullptr);
  ::memcpy(ptr_, bytes, size_);
  ptr_[size_] = '\0';

  env->ReleaseStringUTFChars(str, bytes);
}

bool IsValidAddress(jlong address) {
  bool result = static_cast<jlong>(static_cast<uintptr_t>(address)) == address;
  if (!result) {
    LOG_ERROR("Invalid address 0x%" PRIx64, static_cast<uint64_t>(address));
  }
  return result;
}

// Finds the jclass JNI reference corresponding to a given |class_name|.
// |env| is the current JNI environment handle.
// On success, return true and set |*clazz|.
bool InitClassReference(JNIEnv* env, const char* class_name, jclass* clazz) {
  *clazz = env->FindClass(class_name);
  if (!*clazz) {
    LOG_ERROR("Could not find class for %s", class_name);
    return false;
  }
  return true;
}

// Initializes a jfieldID corresponding to the field of a given |clazz|,
// with name |field_name| and signature |field_sig|.
// |env| is the current JNI environment handle.
// On success, return true and set |*field_id|.
bool InitFieldId(JNIEnv* env,
                 jclass clazz,
                 const char* field_name,
                 const char* field_sig,
                 jfieldID* field_id) {
  *field_id = env->GetFieldID(clazz, field_name, field_sig);
  if (!*field_id) {
    LOG_ERROR("Could not find ID for field '%s'", field_name);
    return false;
  }
  LOG_INFO("Found ID %p for field '%s'", *field_id, field_name);
  return true;
}

bool FindWebViewReservation(uintptr_t* out_address, size_t* out_size) {
  // Note: reading /proc/PID/maps or /proc/PID/smaps is inherently racy. Among
  // other things, the kernel provides these guarantees:
  // * Each region record (line) is well formed
  // * If there is something at a given vaddr during the entirety of the life of
  //   the smaps/maps walk, there will be some output for it.
  //
  // In order for the address/size extraction to be safe, these precausions are
  // made in base/android/linker:
  // * Modification of the range is done only after this function exits
  // * The use of the range is avoided if it is not sufficient in size, which
  //   might happen if it gets split
  const char kFileName[] = "/proc/self/maps";
  int fd = HANDLE_EINTR(open(kFileName, O_RDONLY));
  if (fd == -1) {
    PLOG_ERROR("open %s", kFileName);
    return false;
  }

  bool result = FindRegionInOpenFile(fd, out_address, out_size);
  close(fd);
  return result;
}

// Starting with API level 26 (Android O) the following functions from
// libandroid.so should be used to create shared memory regions to ensure
// compatibility with the future versions:
// * ASharedMemory_create()
// * ASharedMemory_setProt()
//
// This is inspired by //third_party/ashmem/ashmem-dev.c, which cannot be
// referenced from the linker library to avoid increasing binary size.
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

bool NativeLibInfo::FindRelroAndLibraryRangesInElf() {
  LOG_INFO("Called for 0x%" PRIxPTR, load_address_);

  // Check that an ELF library starts at the |load_address_|.
  if (memcmp(reinterpret_cast<void*>(load_address_), ELFMAG, SELFMAG) != 0) {
    LOG_ERROR("Wrong magic number");
    return false;
  }
  auto class_type = *reinterpret_cast<uint8_t*>(load_address_ + EI_CLASS);
  if (class_type == ELFCLASS32) {
    LOG_INFO("ELFCLASS32");
  } else if (class_type == ELFCLASS64) {
    LOG_INFO("ELFCLASS64");
  } else {
    LOG_ERROR("Could not determine ELF class");
    return false;
  }

  // Compute the ranges of PT_LOAD segments and the PT_GNU_RELRO. It is possible
  // to reach for the same information by iterating over all loaded libraries
  // and their program headers using dl_iterate_phdr(3). Instead here the
  // iteration goes through the array |e_phoff[e_phnum]| to avoid acquisition of
  // the global lock in Bionic (dlfcn.cpp).
  //
  // The code relies on (1) having RELRO in the PT_GNU_RELRO segment, and (2)
  // the fact that the address *range* occupied by the library is the minimal
  // address range containing all of the PT_LOAD and PT_GNU_RELRO segments.
  // This is a contract between the static linker and the dynamic linker which
  // seems unlikely to get broken. It might break though as a result of
  // post-processing the DSO, which has historically happened for a few
  // occasions (eliminating the unwind tables and splitting the library into
  // DFMs).
  auto min_vaddr = std::numeric_limits<ElfW(Addr)>::max();
  auto min_relro_vaddr = min_vaddr;
  ElfW(Addr) max_vaddr = 0;
  ElfW(Addr) max_relro_vaddr = 0;
  const auto* ehdr = reinterpret_cast<const ElfW(Ehdr)*>(load_address_);
  const auto* phdrs =
      reinterpret_cast<const ElfW(Phdr)*>(load_address_ + ehdr->e_phoff);
  const size_t kPageSize = GetPageSize();
  for (int i = 0; i < ehdr->e_phnum; i++) {
    const ElfW(Phdr)* phdr = &phdrs[i];
    switch (phdr->p_type) {
      case PT_LOAD:
        if (phdr->p_vaddr < min_vaddr)
          min_vaddr = phdr->p_vaddr;
        if (phdr->p_vaddr + phdr->p_memsz > max_vaddr)
          max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        break;
      case PT_GNU_RELRO:
        min_relro_vaddr = PageStart(kPageSize, phdr->p_vaddr);
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

  // Fill out size and RELRO information.
  load_size_ = PageEnd(kPageSize, max_vaddr) - PageStart(kPageSize, min_vaddr);
  relro_size_ = PageEnd(kPageSize, max_relro_vaddr) -
                PageStart(kPageSize, min_relro_vaddr);
  relro_start_ = load_address_ + PageStart(kPageSize, min_relro_vaddr);
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

  // Histogram ChromiumAndroidLinker.ModernLinkerDlopenExtTime that measured the
  // amount of time the ModernLinker spends to run android_dlopen_ext() was
  // removed in July 2023.

  // Determine the library address ranges and the RELRO region.
  if (!FindRelroAndLibraryRangesInElf()) {
    // Fail early if PT_GNU_RELRO is not found. It likely indicates a
    // build misconfiguration.
    LOG_ERROR("Could not find RELRO in the loaded library: %s", path.c_str());
    abort();
  }

  // Histogram ChromiumAndroidLinker.ModernLinkerIteratePhdrTime that measured
  // the amount of time the ModernLinker spends to find the RELRO region using
  // dl_iterate_phdr() was removed in July 2023.

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
    s_relro_sharing_status = RelroSharingStatus::EXTERNAL_RELRO_FD_NOT_PROVIDED;
    return false;
  }

  if (load_address_ == 0) {
    LOG_ERROR("Load address reset. Second attempt to load the library?");
    s_relro_sharing_status = RelroSharingStatus::EXTERNAL_LOAD_ADDRESS_RESET;
    return false;
  }

  if (!FindRelroAndLibraryRangesInElf()) {
    LOG_ERROR("Could not find RELRO from externally provided address: 0x%p",
              reinterpret_cast<void*>(other_lib_info.load_address_));
    s_relro_sharing_status = RelroSharingStatus::EXTERNAL_RELRO_NOT_FOUND;
    return false;
  }

  SharedMemoryFunctions functions;
  if (!functions.IsWorking()) {
    s_relro_sharing_status = RelroSharingStatus::NO_SHMEM_FUNCTIONS;
    return false;
  }
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
    s_relro_sharing_status = RelroSharingStatus::REMAP_FAILED;
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

JNI_BOUNDARY_EXPORT void
Java_org_chromium_base_library_1loader_LinkerJni_nativeFindMemoryRegionAtRandomAddress(
    JNIEnv* env,
    jclass clazz,
    jobject lib_info_obj) {
  LOG_INFO("Entering");
  uintptr_t address;
  size_t size;
  ReserveAddressWithHint(0, &address, &size);
  s_lib_info_fields.SetLoadInfo(env, lib_info_obj, address, size);
}

JNI_BOUNDARY_EXPORT void
Java_org_chromium_base_library_1loader_LinkerJni_nativeReserveMemoryForLibrary(
    JNIEnv* env,
    jclass clazz,
    jobject lib_info_obj) {
  LOG_INFO("Entering");
  uintptr_t address;
  size_t size;
  s_lib_info_fields.GetLoadInfo(env, lib_info_obj, &address, &size);
  ReserveAddressWithHint(address, &address, &size);
  s_lib_info_fields.SetLoadInfo(env, lib_info_obj, address, size);
}

JNI_BOUNDARY_EXPORT jboolean
Java_org_chromium_base_library_1loader_LinkerJni_nativeFindRegionReservedByWebViewZygote(
    JNIEnv* env,
    jclass clazz,
    jobject lib_info_obj) {
  LOG_INFO("Entering");
  uintptr_t address;
  size_t size;
  if (!FindWebViewReservation(&address, &size))
    return false;
  s_lib_info_fields.SetLoadInfo(env, lib_info_obj, address, size);
  return true;
}

JNI_BOUNDARY_EXPORT jboolean
Java_org_chromium_base_library_1loader_LinkerJni_nativeLoadLibrary(
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

JNI_BOUNDARY_EXPORT jboolean
Java_org_chromium_base_library_1loader_LinkerJni_nativeUseRelros(
    JNIEnv* env,
    jclass clazz,
    jlong local_load_address,
    jobject remote_lib_info_obj) {
  LOG_INFO("Entering");
  // Copy the contents from the Java-side LibInfo object.
  NativeLibInfo incoming_lib_info = {env, remote_lib_info_obj};
  if (!incoming_lib_info.CopyFromJavaObject()) {
    s_relro_sharing_status = RelroSharingStatus::CORRUPTED_IN_JAVA;
    return false;
  }

  // Create an empty NativeLibInfo to extract the current information about the
  // loaded library and later compare with the contents of the
  // |incoming_lib_info|.
  NativeLibInfo lib_info = {nullptr, nullptr};
  lib_info.set_load_address(static_cast<uintptr_t>(local_load_address));

  if (!lib_info.CompareRelroAndReplaceItBy(incoming_lib_info)) {
    return false;
  }
  return true;
}

JNI_BOUNDARY_EXPORT jint
Java_org_chromium_base_library_1loader_LinkerJni_nativeGetRelroSharingResult(
    JNIEnv* env,
    jclass clazz) {
  return static_cast<jint>(s_relro_sharing_status);
}

bool LinkerJNIInit(JavaVM* vm, JNIEnv* env) {
  // Find LibInfo field ids.
  if (!s_lib_info_fields.Init(env)) {
    return false;
  }

  s_java_vm = vm;
  return true;
}

}  // namespace chromium_android_linker
