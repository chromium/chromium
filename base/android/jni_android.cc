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
#include "base/debug/debugging_buildflags.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/robolectric_buildflags.h"
#include "third_party/jni_zero/jni_zero.h"

#if BUILDFLAG(IS_ROBOLECTRIC)
#include "base/base_robolectric_jni/JniAndroid_jni.h"  // nogncheck
#else
#include "base/base_jni/JniAndroid_jni.h"
#endif

namespace base {
namespace android {
namespace {

// If disabled, we LOG(FATAL) immediately in native code when faced with an
// uncaught Java exception (historical behavior). If enabled, we give the Java
// uncaught exception handler a chance to handle the exception first, so that
// the crash is (hopefully) seen as a Java crash, not a native crash.
// TODO(crbug.com/40261529): remove this switch once we are confident the
// new behavior is fine.
BASE_FEATURE(kHandleExceptionsInJava,
             "HandleJniExceptionsInJava",
             base::FEATURE_ENABLED_BY_DEFAULT);

jclass g_out_of_memory_error_class = nullptr;

#if !BUILDFLAG(IS_ROBOLECTRIC)
jmethodID g_class_loader_load_class_method_id = nullptr;
// ClassLoader.loadClass() accepts either slashes or dots on Android, but JVM
// requires dots. We could translate, but there is no need to go through
// ClassLoaders in Robolectric anyways.
// https://cs.android.com/search?q=symbol:DexFile_defineClassNative
jclass GetClassFromSplit(JNIEnv* env,
                         const char* class_name,
                         const char* split_name) {
  DCHECK(IsStringASCII(class_name));
  ScopedJavaLocalRef<jstring> j_class_name(env, env->NewStringUTF(class_name));
  return static_cast<jclass>(env->CallObjectMethod(
      GetSplitClassLoader(env, split_name), g_class_loader_load_class_method_id,
      j_class_name.obj()));
}

// Must be called before using GetClassFromSplit - we need to set the global,
// and we need to call GetClassLoader at least once to allow the default
// resolver (env->FindClass()) to get our main ClassLoader class instance, which
// we then cache use for all future calls to GetSplitClassLoader.
void PrepareClassLoaders(JNIEnv* env) {
  if (g_class_loader_load_class_method_id == nullptr) {
    GetClassLoader(env);
    ScopedJavaLocalRef<jclass> class_loader_clazz = ScopedJavaLocalRef<jclass>(
        env, env->FindClass("java/lang/ClassLoader"));
    CHECK(!ClearException(env));
    g_class_loader_load_class_method_id =
        env->GetMethodID(class_loader_clazz.obj(), "loadClass",
                         "(Ljava/lang/String;)Ljava/lang/Class;");
    CHECK(!ClearException(env));
  }
}
#endif  // !BUILDFLAG(IS_ROBOLECTRIC)
}  // namespace

LogFatalCallback g_log_fatal_callback_for_testing = nullptr;
const char kUnableToGetStackTraceMessage[] =
    "Unable to retrieve Java caller stack trace as the exception handler is "
    "being re-entered";
const char kReetrantOutOfMemoryMessage[] =
    "While handling an uncaught Java exception, an OutOfMemoryError "
    "occurred.";
const char kReetrantExceptionMessage[] =
    "While handling an uncaught Java exception, another exception "
    "occurred.";
const char kUncaughtExceptionMessage[] =
    "Uncaught Java exception in native code. Please include the Java exception "
    "stack from the Android log in your crash report.";
const char kUncaughtExceptionHandlerFailedMessage[] =
    "Uncaught Java exception in native code and the Java uncaught exception "
    "handler did not terminate the process. Please include the Java exception "
    "stack from the Android log in your crash report.";
const char kOomInGetJavaExceptionInfoMessage[] =
    "Unable to obtain Java stack trace due to OutOfMemoryError";

void InitVM(JavaVM* vm) {
  jni_zero::InitVM(vm);
  jni_zero::SetExceptionHandler(CheckException);
  JNIEnv* env = jni_zero::AttachCurrentThread();
#if !BUILDFLAG(IS_ROBOLECTRIC)
  // Warm-up needed for GetClassFromSplit, must be called before we set the
  // resolver, since GetClassFromSplit won't work until after
  // PrepareClassLoaders has happened.
  PrepareClassLoaders(env);
  jni_zero::SetClassResolver(GetClassFromSplit);
#endif
  g_out_of_memory_error_class = static_cast<jclass>(
      env->NewGlobalRef(env->FindClass("java/lang/OutOfMemoryError")));
  DCHECK(g_out_of_memory_error_class);
}


void CheckException(JNIEnv* env) {
  if (!jni_zero::HasException(env)) {
    return;
  }

  static thread_local bool g_reentering = false;
  if (g_reentering) {
    // We were handling an uncaught Java exception already, but one of the Java
    // methods we called below threw another exception. (This is unlikely to
    // happen, as we are careful to never throw from these methods, but we
    // can't rule it out entirely. E.g. an OutOfMemoryError when constructing
    // the jstring for the return value of
    // sanitizedStacktraceForUnhandledException().
    env->ExceptionDescribe();
    jthrowable raw_throwable = env->ExceptionOccurred();
    env->ExceptionClear();
    jclass clazz = env->GetObjectClass(raw_throwable);
    bool is_oom_error = env->IsSameObject(clazz, g_out_of_memory_error_class);
    env->Throw(raw_throwable);  // Ensure we don't re-enter Java.

    if (is_oom_error) {
      base::android::SetJavaException(kReetrantOutOfMemoryMessage);
      // Use different LOG(FATAL) statements to ensure unique stack traces.
      if (g_log_fatal_callback_for_testing) {
        g_log_fatal_callback_for_testing(kReetrantOutOfMemoryMessage);
      } else {
        LOG(FATAL) << kReetrantOutOfMemoryMessage;
      }
    } else {
      base::android::SetJavaException(kReetrantExceptionMessage);
      if (g_log_fatal_callback_for_testing) {
        g_log_fatal_callback_for_testing(kReetrantExceptionMessage);
      } else {
        LOG(FATAL) << kReetrantExceptionMessage;
      }
    }
    // Needed for tests, which do not terminate from LOG(FATAL).
    return;
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
    if (g_log_fatal_callback_for_testing) {
      g_log_fatal_callback_for_testing(kUncaughtExceptionMessage);
    } else {
      LOG(FATAL) << kUncaughtExceptionMessage;
    }
    // Needed for tests, which do not terminate from LOG(FATAL).
    g_reentering = false;
    return;
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
  if (g_log_fatal_callback_for_testing) {
    g_log_fatal_callback_for_testing(kUncaughtExceptionHandlerFailedMessage);
  } else {
    LOG(FATAL) << kUncaughtExceptionHandlerFailedMessage;
  }
  // Needed for tests, which do not terminate from LOG(FATAL).
  g_reentering = false;
}

std::string GetJavaExceptionInfo(JNIEnv* env,
                                 const JavaRef<jthrowable>& throwable) {
  ScopedJavaLocalRef<jstring> sanitized_exception_string =
      Java_JniAndroid_sanitizedStacktraceForUnhandledException(env, throwable);
  // Returns null when PiiElider results in an OutOfMemoryError.
  return sanitized_exception_string
             ? ConvertJavaStringToUTF8(sanitized_exception_string)
             : kOomInGetJavaExceptionInfoMessage;
}

std::string GetJavaStackTraceIfPresent() {
  JNIEnv* env = nullptr;
  JavaVM* jvm = jni_zero::GetVM();
  if (jvm) {
    jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_2);
  }
  if (!env) {
    // JNI has not been initialized on this thread.
    return {};
  }

  if (HasException(env)) {
    // This can happen if CheckException() is being re-entered, decided to
    // LOG(FATAL) immediately, and LOG(FATAL) itself is calling us. In that case
    // it is imperative that we don't try to call Java again.
    return kUnableToGetStackTraceMessage;
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
