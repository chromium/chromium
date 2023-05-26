// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include <stddef.h>
#include <sys/prctl.h>

#include "base/android/java_exception_reporter.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/base_jni/PiiElider_jni.h"
#include "base/debug/debugging_buildflags.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {
namespace android {
namespace {

JavaVM* g_jvm = nullptr;
jobject g_class_loader = nullptr;
jmethodID g_class_loader_load_class_method_id = 0;

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
ABSL_CONST_INIT thread_local void* stack_frame_pointer = nullptr;
#endif

bool g_fatal_exception_occurred = false;

ScopedJavaLocalRef<jclass> GetClassInternal(JNIEnv* env,
                                            const char* class_name,
                                            jobject class_loader) {
  jclass clazz;
  if (class_loader != nullptr) {
    // ClassLoader.loadClass expects a classname with components separated by
    // dots instead of the slashes that JNIEnv::FindClass expects. The JNI
    // generator generates names with slashes, so we have to replace them here.
    // TODO(torne): move to an approach where we always use ClassLoader except
    // for the special case of base::android::GetClassLoader(), and change the
    // JNI generator to generate dot-separated names. http://crbug.com/461773
    size_t bufsize = strlen(class_name) + 1;
    char dotted_name[bufsize];
    memmove(dotted_name, class_name, bufsize);
    for (size_t i = 0; i < bufsize; ++i) {
      if (dotted_name[i] == '/') {
        dotted_name[i] = '.';
      }
    }

    clazz = static_cast<jclass>(
        env->CallObjectMethod(class_loader, g_class_loader_load_class_method_id,
                              ConvertUTF8ToJavaString(env, dotted_name).obj()));
  } else {
    clazz = env->FindClass(class_name);
  }
  if (ClearException(env) || !clazz) {
    LOG(FATAL) << "Failed to find class " << class_name;
  }
  return ScopedJavaLocalRef<jclass>(env, clazz);
}

}  // namespace

JNIEnv* AttachCurrentThread() {
  DCHECK(g_jvm);
  JNIEnv* env = nullptr;
  jint ret = g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
  if (ret == JNI_EDETACHED || !env) {
    JavaVMAttachArgs args;
    args.version = JNI_VERSION_1_2;
    args.group = nullptr;

    // 16 is the maximum size for thread names on Android.
    char thread_name[16];
    int err = prctl(PR_GET_NAME, thread_name);
    if (err < 0) {
      DPLOG(ERROR) << "prctl(PR_GET_NAME)";
      args.name = nullptr;
    } else {
      args.name = thread_name;
    }

#if BUILDFLAG(IS_ANDROID)
    ret = g_jvm->AttachCurrentThread(&env, &args);
#else
    ret = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), &args);
#endif
    CHECK_EQ(JNI_OK, ret);
  }
  return env;
}

JNIEnv* AttachCurrentThreadWithName(const std::string& thread_name) {
  DCHECK(g_jvm);
  JavaVMAttachArgs args;
  args.version = JNI_VERSION_1_2;
  args.name = const_cast<char*>(thread_name.c_str());
  args.group = nullptr;
  JNIEnv* env = nullptr;
#if BUILDFLAG(IS_ANDROID)
  jint ret = g_jvm->AttachCurrentThread(&env, &args);
#else
  jint ret = g_jvm->AttachCurrentThread(reinterpret_cast<void**>(&env), &args);
#endif
  CHECK_EQ(JNI_OK, ret);
  return env;
}

void DetachFromVM() {
  // Ignore the return value, if the thread is not attached, DetachCurrentThread
  // will fail. But it is ok as the native thread may never be attached.
  if (g_jvm)
    g_jvm->DetachCurrentThread();
}

void InitVM(JavaVM* vm) {
  DCHECK(!g_jvm || g_jvm == vm);
  g_jvm = vm;
}

bool IsVMInitialized() {
  return g_jvm != nullptr;
}

JavaVM* GetVM() {
  return g_jvm;
}

void InitGlobalClassLoader(JNIEnv* env) {
  DCHECK(g_class_loader == nullptr);

  ScopedJavaLocalRef<jclass> class_loader_clazz =
      GetClass(env, "java/lang/ClassLoader");
  CHECK(!ClearException(env));
  g_class_loader_load_class_method_id =
      env->GetMethodID(class_loader_clazz.obj(),
                       "loadClass",
                       "(Ljava/lang/String;)Ljava/lang/Class;");
  CHECK(!ClearException(env));

  // GetClassLoader() caches the reference, so we do not need to wrap it in a
  // smart pointer as well.
  g_class_loader = GetClassLoader(env);
}

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env,
                                    const char* class_name,
                                    const char* split_name) {
  return GetClassInternal(env, class_name,
                          GetSplitClassLoader(env, split_name));
}

ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env, const char* class_name) {
  return GetClassInternal(env, class_name, g_class_loader);
}

// This is duplicated with LazyGetClass below because these are performance
// sensitive.
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    const char* split_name,
                    std::atomic<jclass>* atomic_class_id) {
  const jclass value = atomic_class_id->load(std::memory_order_acquire);
  if (value)
    return value;
  ScopedJavaGlobalRef<jclass> clazz;
  clazz.Reset(GetClass(env, class_name, split_name));
  jclass cas_result = nullptr;
  if (atomic_class_id->compare_exchange_strong(cas_result, clazz.obj(),
                                               std::memory_order_acq_rel)) {
    // We intentionally leak the global ref since we now storing it as a raw
    // pointer in |atomic_class_id|.
    return clazz.Release();
  } else {
    return cas_result;
  }
}

// This is duplicated with LazyGetClass above because these are performance
// sensitive.
jclass LazyGetClass(JNIEnv* env,
                    const char* class_name,
                    std::atomic<jclass>* atomic_class_id) {
  const jclass value = atomic_class_id->load(std::memory_order_acquire);
  if (value)
    return value;
  ScopedJavaGlobalRef<jclass> clazz;
  clazz.Reset(GetClass(env, class_name));
  jclass cas_result = nullptr;
  if (atomic_class_id->compare_exchange_strong(cas_result, clazz.obj(),
                                               std::memory_order_acq_rel)) {
    // We intentionally leak the global ref since we now storing it as a raw
    // pointer in |atomic_class_id|.
    return clazz.Release();
  } else {
    return cas_result;
  }
}

template<MethodID::Type type>
jmethodID MethodID::Get(JNIEnv* env,
                        jclass clazz,
                        const char* method_name,
                        const char* jni_signature) {
  auto get_method_ptr = type == MethodID::TYPE_STATIC ?
      &JNIEnv::GetStaticMethodID :
      &JNIEnv::GetMethodID;
  jmethodID id = (env->*get_method_ptr)(clazz, method_name, jni_signature);
  if (base::android::ClearException(env) || !id) {
    LOG(FATAL) << "Failed to find " <<
        (type == TYPE_STATIC ? "static " : "") <<
        "method " << method_name << " " << jni_signature;
  }
  return id;
}

// If |atomic_method_id| set, it'll return immediately. Otherwise, it'll call
// into ::Get() above. If there's a race, it's ok since the values are the same
// (and the duplicated effort will happen only once).
template <MethodID::Type type>
jmethodID MethodID::LazyGet(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature,
                            std::atomic<jmethodID>* atomic_method_id) {
  const jmethodID value = atomic_method_id->load(std::memory_order_acquire);
  if (value)
    return value;
  jmethodID id = MethodID::Get<type>(env, clazz, method_name, jni_signature);
  atomic_method_id->store(id, std::memory_order_release);
  return id;
}

// Various template instantiations.
template jmethodID MethodID::Get<MethodID::TYPE_STATIC>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::Get<MethodID::TYPE_INSTANCE>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature);

template jmethodID MethodID::LazyGet<MethodID::TYPE_STATIC>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature, std::atomic<jmethodID>* atomic_method_id);

template jmethodID MethodID::LazyGet<MethodID::TYPE_INSTANCE>(
    JNIEnv* env, jclass clazz, const char* method_name,
    const char* jni_signature, std::atomic<jmethodID>* atomic_method_id);

bool HasException(JNIEnv* env) {
  return env->ExceptionCheck() != JNI_FALSE;
}

bool ClearException(JNIEnv* env) {
  if (!HasException(env))
    return false;
  env->ExceptionDescribe();
  env->ExceptionClear();
  return true;
}

void CheckException(JNIEnv* env) {
  if (!HasException(env))
    return;

  jthrowable java_throwable = env->ExceptionOccurred();
  if (java_throwable) {
    // Clear the pending exception, since a local reference is now held.
    env->ExceptionDescribe();
    env->ExceptionClear();

    if (g_fatal_exception_occurred) {
      // Another exception (probably OOM) occurred during GetJavaExceptionInfo.
      base::android::SetJavaException(
          "Java OOM'ed in exception handling, check logcat");
    } else {
      g_fatal_exception_occurred = true;
      // RVO should avoid any extra copies of the exception string.
      base::android::SetJavaException(
          GetJavaExceptionInfo(env, java_throwable).c_str());
    }
  }

  // Now, feel good about it and die.
  LOG(FATAL) << "Please include Java exception stack in crash report";
}

std::string GetJavaExceptionInfo(JNIEnv* env, jthrowable java_throwable) {
  ScopedJavaLocalRef<jstring> sanitized_exception_string =
      Java_PiiElider_getSanitizedStacktrace(
          env, ScopedJavaLocalRef(env, java_throwable));

  return ConvertJavaStringToUTF8(sanitized_exception_string);
}

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

JNIStackFrameSaver::JNIStackFrameSaver(void* current_fp)
    : resetter_(&stack_frame_pointer, current_fp) {}

JNIStackFrameSaver::~JNIStackFrameSaver() = default;

void* JNIStackFrameSaver::SavedFrame() {
  return stack_frame_pointer;
}

#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

}  // namespace android
}  // namespace base
