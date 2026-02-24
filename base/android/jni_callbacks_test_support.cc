// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_junit_tests_jni/JniCallbacksTest_jni.h"

namespace base::android {

namespace {
int g_once_closure_run_count = 0;
bool g_once_callback_result = false;
int g_repeating_closure_run_count = 0;
int g_repeating_callback_result_count = 0;
bool g_once_callback2_result1 = false;
int g_once_callback2_result2 = 0;
int g_repeating_callback2_result_count = 0;
}  // namespace

static void JNI_JniCallbacksTest_ResetCounters(JNIEnv* env) {
  g_once_closure_run_count = 0;
  g_once_callback_result = false;
  g_repeating_closure_run_count = 0;
  g_repeating_callback_result_count = 0;
  g_once_callback2_result1 = false;
  g_once_callback2_result2 = 0;
  g_repeating_callback2_result_count = 0;
}

static base::OnceClosure JNI_JniCallbacksTest_GetOnceClosure() {
  return base::BindOnce([]() { g_once_closure_run_count++; });
}

static jint JNI_JniCallbacksTest_GetOnceClosureRunCount() {
  return g_once_closure_run_count;
}

static base::OnceCallback<void(bool)> JNI_JniCallbacksTest_GetOnceCallback() {
  return base::BindOnce([](bool r) { g_once_callback_result = r; });
}

static jboolean JNI_JniCallbacksTest_GetOnceCallbackResult() {
  return g_once_callback_result;
}

static base::RepeatingClosure JNI_JniCallbacksTest_GetRepeatingClosure() {
  return base::BindRepeating([]() { g_repeating_closure_run_count++; });
}

static jint JNI_JniCallbacksTest_GetRepeatingClosureRunCount() {
  return g_repeating_closure_run_count;
}

static base::RepeatingCallback<void(bool)>
JNI_JniCallbacksTest_GetRepeatingCallback() {
  return base::BindRepeating(
      [](bool r) { g_repeating_callback_result_count++; });
}

static jint JNI_JniCallbacksTest_GetRepeatingCallbackResultCount() {
  return g_repeating_callback_result_count;
}

static base::OnceCallback<void(bool, int32_t)>
JNI_JniCallbacksTest_GetOnceCallback2() {
  return base::BindOnce([](bool r1, int32_t r2) {
    g_once_callback2_result1 = r1;
    g_once_callback2_result2 = r2;
  });
}

static jboolean JNI_JniCallbacksTest_GetOnceCallback2Result1() {
  return g_once_callback2_result1;
}

static jint JNI_JniCallbacksTest_GetOnceCallback2Result2() {
  return g_once_callback2_result2;
}

static base::RepeatingCallback<void(bool, int32_t)>
JNI_JniCallbacksTest_GetRepeatingCallback2() {
  return base::BindRepeating(
      [](bool r1, int32_t r2) { g_repeating_callback2_result_count++; });
}

static jint JNI_JniCallbacksTest_GetRepeatingCallback2ResultCount() {
  return g_repeating_callback2_result_count;
}

static void JNI_JniCallbacksTest_PassOnceClosure(base::OnceClosure closure) {
  std::move(closure).Run();
}

static void JNI_JniCallbacksTest_PassOnceCallback(
    base::OnceCallback<void(int32_t)> callback) {
  std::move(callback).Run(42);
}

static void JNI_JniCallbacksTest_PassRepeatingClosure(
    base::RepeatingClosure closure) {
  closure.Run();
  closure.Run();
}

static void JNI_JniCallbacksTest_PassRepeatingCallback(
    base::RepeatingCallback<void(int32_t)> callback) {
  callback.Run(1);
  callback.Run(2);
}

static void JNI_JniCallbacksTest_PassOnceCallback2(
    base::OnceCallback<void(bool, int32_t)> callback) {
  std::move(callback).Run(true, 100);
}

static void JNI_JniCallbacksTest_PassRepeatingCallback2(
    base::RepeatingCallback<void(bool, int32_t)> callback) {
  callback.Run(true, 1);
  callback.Run(false, 2);
}

}  // namespace base::android

DEFINE_JNI(JniCallbacksTest)
