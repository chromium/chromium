// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_junit_tests_jni/JniCallbacksTest_jni.h"

namespace base::android {

namespace {
int g_run_count = 0;
bool g_once_callback_result = false;
bool g_once_callback2_result1 = false;
int g_once_callback2_result2 = 0;

std::string& GetOnceCallbackWithSubtypeResult() {
  static base::NoDestructor<std::string> instance;
  return *instance;
}
}  // namespace

static void JNI_JniCallbacksTest_TriggerOnceClosure(JNIEnv* env) {
  base::OnceClosure closure = base::BindOnce([]() { g_run_count++; });
  Java_JniCallbacksTest_callOnceClosure(env, std::move(closure));
}

static void JNI_JniCallbacksTest_TriggerOnceCallback(JNIEnv* env) {
  base::OnceCallback<void(int32_t)> callback =
      base::BindOnce([](int32_t r) { g_once_callback2_result2 = r; });
  Java_JniCallbacksTest_callOnceCallback(env, std::move(callback));
}

static void JNI_JniCallbacksTest_TriggerOnceCallback2(JNIEnv* env) {
  base::OnceCallback<void(bool, int32_t)> callback =
      base::BindOnce([](bool r1, int32_t r2) {
        g_once_callback2_result1 = r1;
        g_once_callback2_result2 = r2;
      });
  Java_JniCallbacksTest_callOnceCallback2(env, std::move(callback));
}

static void JNI_JniCallbacksTest_TriggerRepeatingClosure(JNIEnv* env) {
  base::RepeatingClosure closure = base::BindRepeating([]() { g_run_count++; });
  Java_JniCallbacksTest_callRepeatingClosure(env, closure);
}

static void JNI_JniCallbacksTest_TriggerRepeatingClosureMoveOnly(JNIEnv* env) {
  base::RepeatingClosure closure = base::BindRepeating([]() { g_run_count++; });
  Java_JniCallbacksTest_callRepeatingClosure(env, std::move(closure));
}

static void JNI_JniCallbacksTest_TriggerRepeatingCallback(JNIEnv* env) {
  // Test using std::move() with a const& param.
  base::RepeatingCallback<void(int32_t)> callback =
      base::BindRepeating([](int32_t r) { g_run_count++; });
  Java_JniCallbacksTest_callRepeatingCallback(env, std::move(callback));
}

static void JNI_JniCallbacksTest_TriggerRepeatingCallback2(JNIEnv* env) {
  base::RepeatingCallback<void(bool, int32_t)> callback =
      base::BindRepeating([](bool r1, int32_t r2) { g_run_count++; });
  Java_JniCallbacksTest_callRepeatingCallback2(env, callback);
}

static void JNI_JniCallbacksTest_ResetCounters(JNIEnv* env) {
  g_run_count = 0;
  g_once_callback_result = false;
  g_once_callback2_result1 = false;
  g_once_callback2_result2 = 0;
  GetOnceCallbackWithSubtypeResult().clear();
}

static base::OnceClosure JNI_JniCallbacksTest_GetOnceClosure(JNIEnv* env) {
  return base::BindOnce([]() { g_run_count++; });
}

static jint JNI_JniCallbacksTest_GetRunCount(JNIEnv* env) {
  return g_run_count;
}

static base::OnceCallback<void(bool)> JNI_JniCallbacksTest_GetOnceCallback(
    JNIEnv* env) {
  return base::BindOnce([](bool r) { g_once_callback_result = r; });
}

static jboolean JNI_JniCallbacksTest_GetOnceCallbackResult(JNIEnv* env) {
  return g_once_callback_result;
}

static base::RepeatingClosure JNI_JniCallbacksTest_GetRepeatingClosure(
    JNIEnv* env) {
  return base::BindRepeating([]() { g_run_count++; });
}

static base::RepeatingCallback<void(bool)>
JNI_JniCallbacksTest_GetRepeatingCallback(JNIEnv* env) {
  return base::BindRepeating([](bool r) { g_run_count++; });
}

static base::OnceCallback<void(bool, const jni_zero::JavaRef<jobject>&)>
JNI_JniCallbacksTest_GetOnceCallback2(JNIEnv* env) {
  return base::BindOnce([](bool r1, const jni_zero::JavaRef<jobject>& r2) {
    g_once_callback2_result1 = r1;
    g_once_callback2_result2 =
        jni_zero::FromJniType<int32_t>(jni_zero::AttachCurrentThread(), r2);
  });
}

static jboolean JNI_JniCallbacksTest_GetOnceCallback2Result1(JNIEnv* env) {
  return g_once_callback2_result1;
}

static jint JNI_JniCallbacksTest_GetOnceCallback2Result2(JNIEnv* env) {
  return g_once_callback2_result2;
}

static base::RepeatingCallback<void(bool, int32_t)>
JNI_JniCallbacksTest_GetRepeatingCallback2(JNIEnv* env) {
  return base::BindRepeating([](bool r1, int32_t r2) { g_run_count++; });
}

static base::OnceCallback<void(const jni_zero::JavaRef<jstring>&)>
JNI_JniCallbacksTest_GetOnceCallbackWithSubtype(JNIEnv* env) {
  return base::BindOnce([](const jni_zero::JavaRef<jstring>& r) {
    GetOnceCallbackWithSubtypeResult() =
        ConvertJavaStringToUTF8(jni_zero::AttachCurrentThread(), r);
  });
}

static ScopedJavaLocalRef<jstring>
JNI_JniCallbacksTest_GetOnceCallbackWithSubtypeResult(JNIEnv* env) {
  return ConvertUTF8ToJavaString(env, GetOnceCallbackWithSubtypeResult());
}

static base::RepeatingCallback<void(const jni_zero::JavaRef<jstring>&)>
JNI_JniCallbacksTest_GetRepeatingCallbackWithSubtype(JNIEnv* env) {
  return base::BindRepeating(
      [](const jni_zero::JavaRef<jstring>& r) { g_run_count++; });
}

static void JNI_JniCallbacksTest_PassOnceClosure(JNIEnv* env,
                                                 base::OnceClosure closure) {
  std::move(closure).Run();
}

static void JNI_JniCallbacksTest_PassOnceCallback(
    JNIEnv* env,
    base::OnceCallback<void(int32_t)> callback) {
  std::move(callback).Run(42);
}

static void JNI_JniCallbacksTest_PassRepeatingClosure(
    JNIEnv* env,
    base::RepeatingClosure closure) {
  closure.Run();
  closure.Run();
}

static void JNI_JniCallbacksTest_PassRepeatingCallback(
    JNIEnv* env,
    base::RepeatingCallback<void(int32_t)> callback) {
  callback.Run(1);
  callback.Run(2);
}

static void JNI_JniCallbacksTest_PassOnceCallback2(
    JNIEnv* env,
    base::OnceCallback<void(bool, int32_t)> callback) {
  std::move(callback).Run(true, 100);
}

static void JNI_JniCallbacksTest_PassRepeatingCallback2(
    JNIEnv* env,
    base::RepeatingCallback<void(bool, int32_t)> callback) {
  callback.Run(true, 1);
  callback.Run(false, 2);
}

static void JNI_JniCallbacksTest_PassOnceCallbackWithSubtype(
    JNIEnv* env,
    base::OnceCallback<void(const jni_zero::JavaRef<jstring>&)> callback) {
  std::move(callback).Run(ConvertUTF8ToJavaString(env, "test string"));
}

static void JNI_JniCallbacksTest_PassOnceCallbackWithScopedSubtype(
    JNIEnv* env,
    base::OnceCallback<void(jni_zero::ScopedJavaLocalRef<jstring>)> callback) {
  std::move(callback).Run(ConvertUTF8ToJavaString(env, "scoped string"));
}

static void JNI_JniCallbacksTest_PassRepeatingCallbackWithSubtype(
    JNIEnv* env,
    base::RepeatingCallback<void(const jni_zero::JavaRef<jstring>&)> callback) {
  callback.Run(ConvertUTF8ToJavaString(env, "s1"));
  callback.Run(ConvertUTF8ToJavaString(env, "s2"));
}

}  // namespace base::android

DEFINE_JNI(JniCallbacksTest)
