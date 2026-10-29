// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/input_hint_checker.h"

#include <jni.h>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_unittest_support_jni/InputHintCheckerTestUtil_jni.h"

namespace base::android {

namespace {

int g_new_local_ref_count = 0;
const JNINativeInterface* g_previous_functions = nullptr;

jobject HookedNewLocalRef(JNIEnv* env, jobject obj) {
  ++g_new_local_ref_count;
  return g_previous_functions->NewLocalRef(env, obj);
}

}  // namespace

TEST(InputHintCheckerTest, RealViewInitialization) {
  if (android_info::sdk_int() < android_info::SDK_VERSION_V) {
    GTEST_SKIP() << "Requires Android V+ (API 35+)";
  }

  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      kYieldWithInputHint, {{"poll_interval_ms", "1000"}});
  InputHintChecker::InitializeFeatures();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> view =
      Java_InputHintCheckerTestUtil_createView(env);
  ASSERT_FALSE(view.is_null());

  // Ensure the global singleton is bound to this thread before
  // ScopedOverrideInstance redirects GetInstance() during off-thread init.
  InputHintChecker::GetInstance();

  InputHintChecker checker;
  InputHintChecker::ScopedOverrideInstance scoped_override(&checker);
  checker.SetView(env, view);

  // Wait for asynchronous initialization to complete.
  ElapsedTimer timer;
  while (!checker.IsInitializedForTesting() &&
         !checker.FailedToInitializeForTesting()) {
    ASSERT_LT(timer.Elapsed(), TestTimeouts::action_timeout())
        << "Timed out waiting for InputHintChecker initialization.";
    PlatformThread::Sleep(Milliseconds(5));
  }
  ASSERT_TRUE(checker.IsInitializedForTesting());
  EXPECT_FALSE(checker.FailedToInitializeForTesting());

  // Verify calling HasInputImpl succeeds without JNI exception.
  bool hint = checker.HasInputImplNoThrottlingForTesting(env);
  EXPECT_FALSE(checker.FailedToInitializeForTesting());
  EXPECT_FALSE(hint);

  JNINativeInterface hooked_functions = *env->functions;
  g_previous_functions = env->functions;
  hooked_functions.NewLocalRef = &HookedNewLocalRef;
  env->functions = &hooked_functions;

  // Uninitialized view returns early before promoting the weak reference.
  checker.SetView(env, nullptr);
  g_new_local_ref_count = 0;
  EXPECT_FALSE(checker.HasInputImplWithThrottlingForTesting(env));
  EXPECT_EQ(g_new_local_ref_count, 0);

  // First unthrottled check promotes the weak reference once.
  checker.SetView(env, view);
  EXPECT_FALSE(checker.HasInputImplWithThrottlingForTesting(env));
  EXPECT_EQ(g_new_local_ref_count, 1);

  // Immediate subsequent check is throttled before promoting the weak
  // reference.
  EXPECT_FALSE(checker.HasInputImplWithThrottlingForTesting(env));
  EXPECT_EQ(g_new_local_ref_count, 1);

  env->functions = g_previous_functions;
  g_previous_functions = nullptr;
  feature_list.Reset();
  InputHintChecker::InitializeFeatures();
}

}  // namespace base::android

DEFINE_JNI(InputHintCheckerTestUtil)
