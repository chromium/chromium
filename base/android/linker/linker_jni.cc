// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a part of the Android-specific Chromium dynamic linker.
//
// See linker_jni.h for more details and the dependency rules.

#include "base/android/linker/linker_jni.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <memory>

namespace chromium_android_linker {

// Variable containing LibInfo for the loaded library.
LibInfo_class s_lib_info_fields;

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

namespace {

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

  if (vma_start % PAGE_SIZE || vma_end % PAGE_SIZE)
    return false;

  *out_address = static_cast<uintptr_t>(vma_start);
  *out_size = vma_end - vma_start;

  return true;
}

bool FindRegionInOpenFile(int fd, uintptr_t* out_address, size_t* out_size) {
  constexpr size_t kMaxLineLength = 256;
  constexpr size_t kReadSize = PAGE_SIZE;

  // Loop until no bytes left to scan. On every iteration except the last, fill
  // the buffer till the end. On every iteration except the first, the buffer
  // begins with kMaxLineLength bytes from the end of the previous fill.
  char buf[kReadSize + kMaxLineLength + 1];
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
  return false;
}

}  // namespace

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

// Performs as described in Linker.java.
JNI_GENERATOR_EXPORT void
Java_org_chromium_base_library_1loader_LinkerJni_nativeFindMemoryRegionAtRandomAddress(
    JNIEnv* env,
    jclass clazz,
    jobject lib_info_obj,
    jboolean keep_reserved) {
  LOG_INFO("Entering");
  uintptr_t address;
  size_t size;
  ReserveAddressWithHint(0, &address, &size);
  if (!keep_reserved && address != 0) {
    munmap(reinterpret_cast<void*>(address), size);
  }
  s_lib_info_fields.SetLoadInfo(env, lib_info_obj, address, size);
}

// Performs as described in Linker.java.
JNI_GENERATOR_EXPORT void
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

// Performs as described in Linker.java.
JNI_GENERATOR_EXPORT jboolean
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

}  // namespace chromium_android_linker
