// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Android-specific Chromium linker, a tiny shared library
// implementing a custom dynamic linker that can be used to load the
// real Chromium libraries.

// The main point of this linker is to be able to share the RELRO
// section of libchrome.so (or equivalent) between renderer processes.

// This source code *cannot* depend on anything from base/ or the C++
// STL, to keep the final library small, and avoid ugly dependency issues.

#include "base/android/linker/linker_jni.h"

#include <errno.h>
#include <inttypes.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

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

}  // namespace

// Performs as described in Linker.java.
JNI_GENERATOR_EXPORT void
Java_org_chromium_base_library_1loader_Linker_nativeFindMemoryRegionAtRandomAddress(
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
Java_org_chromium_base_library_1loader_Linker_nativeReserveMemoryForLibrary(
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

}  // namespace chromium_android_linker
