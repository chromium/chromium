// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the Android-specific Chromium dynamic linker (loader of dynamic
// libraries), a tiny shared library implementing a custom dynamic linker that
// can be used to load the real Chromium libraries.
//
// The purpose of this custom linker is to be able to share the RELRO section of
// libcontentshell.so (or equivalent) between the browser process and all other
// processes it asks to create.
//
// This source code *cannot* depend on anything from //base or the C++ standard
// library to keep this DSO small and avoid dependency issues. An exception is
// made for std::unique_ptr as a risky header-only definition.

#ifndef BASE_ANDROID_LINKER_LINKER_JNI_H_
#define BASE_ANDROID_LINKER_LINKER_JNI_H_

#include <android/log.h>
#include <jni.h>
#include <stddef.h>
#include <stdlib.h>

#include "build/build_config.h"

// Set this to 1 to enable debug traces to the Android log.
// Note that LOG() from "base/logging.h" cannot be used, since it is
// in base/ which hasn't been loaded yet.
#define DEBUG 0

#define TAG "cr_ChromiumAndroidLinker"

#if DEBUG
#define LOG_INFO(FORMAT, ...)                                             \
  __android_log_print(ANDROID_LOG_INFO, TAG, "%s: " FORMAT, __FUNCTION__, \
                      ##__VA_ARGS__)
#else
#define LOG_INFO(FORMAT, ...) ((void)0)
#endif
#define LOG_ERROR(FORMAT, ...)                                             \
  __android_log_print(ANDROID_LOG_ERROR, TAG, "%s: " FORMAT, __FUNCTION__, \
                      ##__VA_ARGS__)
#define PLOG_ERROR(FORMAT, ...) \
  LOG_ERROR(FORMAT ": %s", ##__VA_ARGS__, strerror(errno))

#define UNUSED __attribute__((unused))

#if defined(ARCH_CPU_X86)
// Dalvik JIT generated code doesn't guarantee 16-byte stack alignment on
// x86 - use force_align_arg_pointer to realign the stack at the JNI
// boundary. https://crbug.com/655248
#define JNI_GENERATOR_EXPORT \
  extern "C" __attribute__((visibility("default"), force_align_arg_pointer))
#else
#define JNI_GENERATOR_EXPORT extern "C" __attribute__((visibility("default")))
#endif

#if defined(__arm__) && defined(__ARM_ARCH_7A__)
#define CURRENT_ABI "armeabi-v7a"
#elif defined(__arm__)
#define CURRENT_ABI "armeabi"
#elif defined(__i386__)
#define CURRENT_ABI "x86"
#elif defined(__mips__)
#define CURRENT_ABI "mips"
#elif defined(__x86_64__)
#define CURRENT_ABI "x86_64"
#elif defined(__aarch64__)
#define CURRENT_ABI "arm64-v8a"
#else
#error "Unsupported target abi"
#endif

#if !defined(PAGE_SIZE)
#define PAGE_SIZE (1 << 12)
#define PAGE_MASK (~(PAGE_SIZE - 1))
#endif

#define PAGE_START(x) ((x)&PAGE_MASK)
#define PAGE_END(x) PAGE_START((x) + (PAGE_SIZE - 1))

// Copied from //base/posix/eintr_wrapper.h to avoid depending on //base.
#define HANDLE_EINTR(x)                                     \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

namespace chromium_android_linker {

// Larger than the largest library we might attempt to load.
static const size_t kAddressSpaceReservationSize = 192 * 1024 * 1024;

// A simple scoped UTF String class that can be initialized from
// a Java jstring handle. Modeled like std::string, which cannot
// be used here.
class String {
 public:
  String(JNIEnv* env, jstring str);

  inline ~String() { ::free(ptr_); }

  inline const char* c_str() const { return ptr_ ? ptr_ : ""; }
  inline size_t size() const { return size_; }

 private:
  char* ptr_;
  size_t size_;
};

// Returns true iff casting a java-side |address| to uintptr_t does not lose
// bits.
bool IsValidAddress(jlong address);

// Find the jclass JNI reference corresponding to a given |class_name|.
// |env| is the current JNI environment handle.
// On success, return true and set |*clazz|.
bool InitClassReference(JNIEnv* env, const char* class_name, jclass* clazz);

// Finds the region reserved by the WebView zygote if the current process is
// inherited from the modern enough zygote that has this reservation. If the
// lookup is successful, returns true and sets |address| and |size|. Otherwise
// returns false.
bool FindWebViewReservation(uintptr_t* address, size_t* size);

// Initialize a jfieldID corresponding to the field of a given |clazz|,
// with name |field_name| and signature |field_sig|.
// |env| is the current JNI environment handle.
// On success, return true and set |*field_id|.
bool InitFieldId(JNIEnv* env,
                 jclass clazz,
                 const char* field_name,
                 const char* field_sig,
                 jfieldID* field_id);

// Initialize a jfieldID corresponding to the static field of a given |clazz|,
// with name |field_name| and signature |field_sig|.
// |env| is the current JNI environment handle.
// On success, return true and set |*field_id|.
bool InitStaticFieldId(JNIEnv* env,
                       jclass clazz,
                       const char* field_name,
                       const char* field_sig,
                       jfieldID* field_id);

// A class used to model the field IDs of the org.chromium.base.Linker
// LibInfo inner class, used to communicate data with the Java side
// of the linker.
struct LibInfo_class {
  jfieldID load_address_id;
  jfieldID load_size_id;
  jfieldID relro_start_id;
  jfieldID relro_size_id;
  jfieldID relro_fd_id;

  // Initialize an instance.
  bool Init(JNIEnv* env) {
    jclass clazz;
    if (!InitClassReference(
            env, "org/chromium/base/library_loader/Linker$LibInfo", &clazz)) {
      return false;
    }

    return InitFieldId(env, clazz, "mLoadAddress", "J", &load_address_id) &&
           InitFieldId(env, clazz, "mLoadSize", "J", &load_size_id) &&
           InitFieldId(env, clazz, "mRelroStart", "J", &relro_start_id) &&
           InitFieldId(env, clazz, "mRelroSize", "J", &relro_size_id) &&
           InitFieldId(env, clazz, "mRelroFd", "I", &relro_fd_id);
  }

  void SetLoadInfo(JNIEnv* env,
                   jobject library_info_obj,
                   uintptr_t load_address,
                   size_t load_size) {
    env->SetLongField(library_info_obj, load_address_id, load_address);
    env->SetLongField(library_info_obj, load_size_id, load_size);
  }

  void SetRelroInfo(JNIEnv* env,
                    jobject library_info_obj,
                    uintptr_t relro_start,
                    size_t relro_size,
                    int relro_fd) {
    env->SetLongField(library_info_obj, relro_start_id, relro_start);
    env->SetLongField(library_info_obj, relro_size_id, relro_size);
    env->SetIntField(library_info_obj, relro_fd_id, relro_fd);
  }

  bool GetLoadInfo(JNIEnv* env,
                   jobject library_info_obj,
                   uintptr_t* load_address,
                   size_t* load_size) {
    if (load_address) {
      jlong java_address = env->GetLongField(library_info_obj, load_address_id);
      if (!IsValidAddress(java_address))
        return false;
      *load_address = static_cast<uintptr_t>(java_address);
    }
    if (load_size) {
      *load_size = static_cast<uintptr_t>(
          env->GetLongField(library_info_obj, load_size_id));
    }
    return true;
  }

  void GetRelroInfo(JNIEnv* env,
                    jobject library_info_obj,
                    uintptr_t* relro_start,
                    size_t* relro_size,
                    int* relro_fd) {
    if (relro_start) {
      *relro_start = static_cast<uintptr_t>(
          env->GetLongField(library_info_obj, relro_start_id));
    }

    if (relro_size) {
      *relro_size = static_cast<size_t>(
          env->GetLongField(library_info_obj, relro_size_id));
    }

    if (relro_fd) {
      *relro_fd = env->GetIntField(library_info_obj, relro_fd_id);
    }
  }
};

// Variable containing LibInfo accessors for the loaded library.
extern LibInfo_class s_lib_info_fields;

jint JNI_OnLoad(JavaVM* vm, void* reserved);

}  // namespace chromium_android_linker

#endif  // BASE_ANDROID_LINKER_LINKER_JNI_H_
