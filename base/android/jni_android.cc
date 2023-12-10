// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include <stddef.h>
#include <sys/prctl.h>

#include "base/android/java_exception_reporter.h"
#include "base/android/jni_string.h"
#include "base/android/jni_utils.h"
#include "base/android_runtime_jni_headers/Throwable_jni.h"
#include "base/base_jni/JniAndroid_jni.h"
#include "base/debug/debugging_buildflags.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/attributes.h"

namespace base {
namespace android {
namespace {

// If disabled, we LOG(FATAL) immediately in native code when faced with an
// uncaught Java exception (historical behavior). If enabled, we give the Java
// uncaught exception handler a chance to handle the exception first, so that
// the crash is (hopefully) seen as a Java crash, not a native crash.
// TODO(https://crbug.com/1426888): remove this switch once we are confident the
// new behavior is fine.
BASE_FEATURE(kHandleExceptionsInJava,
             "HandleJniExceptionsInJava",
             base::FEATURE_ENABLED_BY_DEFAULT);

JavaVM* g_jvm = nullptr;
jobject g_class_loader = nullptr;
jclass g_out_of_memory_error_class = nullptr;
jmethodID g_class_loader_load_class_method_id = nullptr;

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
  JNIEnv* env = base::android::AttachCurrentThread();
  g_out_of_memory_error_class = static_cast<jclass>(
      env->NewGlobalRef(env->FindClass("java/lang/OutOfMemoryError")));
  DCHECK(g_out_of_memory_error_class);
}

bool IsVMInitialized() {
  return g_jvm != nullptr;
}

JavaVM* GetVM() {
  return g_jvm;
}

void DisableJvmForTesting() {
  g_jvm = nullptr;
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

  static thread_local bool g_reentering = false;
  if (g_reentering) {
    // We were handling an uncaught Java exception already, but one of the Java
    // methods we called below threw another exception. (This is unlikely to
    // happen as we are careful to never throw from these methods, but we can't
    // rule it out entirely as the JVM itself may throw - think
    // OutOfMemoryError, for example.)
    env->ExceptionDescribe();
    jthrowable raw_throwable = env->ExceptionOccurred();
    env->ExceptionClear();
    jclass clazz = env->GetObjectClass(raw_throwable);
    bool is_oom_error = env->IsSameObject(clazz, g_out_of_memory_error_class);
    env->Throw(raw_throwable);  // Ensure we don't re-enter Java.

    if (is_oom_error) {
      constexpr char kMessage[] =
          "While handling an uncaught Java exception, an OutOfMemoryError "
          "occurred.";
      base::android::SetJavaException(kMessage);
      // Use different LOG(FATAL) statements to ensure unique stack traces.
      LOG(FATAL) << kMessage;
    } else {
      constexpr char kMessage[] =
          "While handling an uncaught Java exception, another exception "
          "occurred.";
      base::android::SetJavaException(kMessage);
      LOG(FATAL) << kMessage;
    }
  }
  g_reentering = true;

  // Log a message to ensure there is something in the log even if the rest of
  // this function goes horribly wrong, and also to provide a convenient marker
  // in the log for where Java exception crash information starts.
  LOG(ERROR) << "Crashing due to uncaught Java exception";

  const bool handle_exception_in_java =
      base::FeatureList::IsEnabled(kHandleExceptionsInJava);

  if (!handle_exception_in_java) {
    env->ExceptionDescribe();
  }

  // We cannot use `ScopedJavaLocalRef` directly because that ends up calling
  // env->GetObjectRefType() when DCHECK is on, and that call is not allowed
  // with a pending exception according to the JNI spec.
  jthrowable raw_throwable = env->ExceptionOccurred();
  // Now that we saved the reference to the throwable, clear the exception.
  //
  // We need to do this as early as possible to remove the risk that code below
  // might accidentally call back into Java, which is not allowed when `env`
  // has an exception set, per the JNI spec. (For example, LOG(FATAL) doesn't
  // work with a JNI exception set, because it calls
  // GetJavaStackTraceIfPresent()).
  env->ExceptionClear();
  // The reference returned by `ExceptionOccurred()` is a local reference.
  // `ExceptionClear()` merely removes the exception information from `env`;
  // it doesn't delete the reference, which is why this call is valid.
  auto throwable = ScopedJavaLocalRef<jthrowable>::Adopt(env, raw_throwable);

  if (!handle_exception_in_java) {
    base::android::SetJavaException(
        GetJavaExceptionInfo(env, throwable).c_str());
    LOG(FATAL)
        << "Uncaught Java exception in native code. Please include the Java "
           "exception stack from the Android log in your crash report.";
  }

  // We don't need to call SetJavaException() in this branch because we
  // expect handleException() to eventually call JavaExceptionReporter through
  // the global uncaught exception handler.

  const std::string native_stack_trace = base::debug::StackTrace().ToString();
  LOG(ERROR) << "Native stack trace:" << std::endl << native_stack_trace;

  ScopedJavaLocalRef<jthrowable> secondary_exception =
      Java_JniAndroid_handleException(
          env, throwable, ConvertUTF8ToJavaString(env, native_stack_trace));

  // Ideally handleException() should have terminated the process and we should
  // not get here. This can happen in the case of OutOfMemoryError or if the
  // app that embedded WebView installed an exception handler that does not
  // terminate, or itself threw an exception. We cannot be confident that
  // JavaExceptionReporter ran, so set the java exception explicitly.
  base::android::SetJavaException(
      GetJavaExceptionInfo(
          env, secondary_exception ? secondary_exception : throwable)
          .c_str());
  LOG(FATAL)
      << "Uncaught Java exception in native code, and the Java uncaught "
         "exception handler did not terminate the process. Please include the "
         "Java exception stack from the Android log in your crash report.";
}

std::string GetJavaExceptionInfo(JNIEnv* env,
                                 const JavaRef<jthrowable>& throwable) {
  ScopedJavaLocalRef<jstring> sanitized_exception_string =
      Java_JniAndroid_sanitizedStacktraceForUnhandledException(env, throwable);
  // Returns null when PiiElider results in an OutOfMemoryError.
  return sanitized_exception_string
             ? ConvertJavaStringToUTF8(sanitized_exception_string)
             : "Unable to obtain Java stack trace due to OutOfMemoryError";
}

std::string GetJavaStackTraceIfPresent() {
  JNIEnv* env = nullptr;
  if (g_jvm) {
    g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
  }
  if (!env) {
    // JNI has not been initialized on this thread.
    return {};
  }

  if (HasException(env)) {
    // This can happen if CheckException() is being re-entered, decided to
    // LOG(FATAL) immediately, and LOG(FATAL) itself is calling us. In that case
    // it is imperative that we don't try to call Java again.
    return "Unable to retrieve Java caller stack trace as the exception "
           "handler is being re-entered";
  }

  ScopedJavaLocalRef<jthrowable> throwable =
      JNI_Throwable::Java_Throwable_Constructor(env);
  std::string ret = GetJavaExceptionInfo(env, throwable);
  // Strip the exception message and leave only the "at" lines. Example:
  // java.lang.Throwable:
  // {tab}at Clazz.method(Clazz.java:111)
  // {tab}at ...
  size_t newline_idx = ret.find('\n');
  if (newline_idx == std::string::npos) {
    // There are no java frames.
    return {};
  }
  return ret.substr(newline_idx + 1);
}

}  // namespace android
}  // namespace base
