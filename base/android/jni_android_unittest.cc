// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StartsWith;

namespace base {
namespace android {

namespace {

std::atomic<jmethodID> g_atomic_id(nullptr);
int LazyMethodIDCall(JNIEnv* env, jclass clazz, int p) {
  jmethodID id = base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, clazz,
      "abs",
      "(I)I",
      &g_atomic_id);

  return env->CallStaticIntMethod(clazz, id, p);
}

int MethodIDCall(JNIEnv* env, jclass clazz, jmethodID id, int p) {
  return env->CallStaticIntMethod(clazz, id, p);
}

}  // namespace

TEST(JNIAndroidMicrobenchmark, MethodId) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jclass> clazz(GetClass(env, "java/lang/Math"));
  base::Time start_lazy = base::Time::Now();
  int o = 0;
  for (int i = 0; i < 1024; ++i)
    o += LazyMethodIDCall(env, clazz.obj(), i);
  base::Time end_lazy = base::Time::Now();

  jmethodID id = g_atomic_id;
  base::Time start = base::Time::Now();
  for (int i = 0; i < 1024; ++i)
    o += MethodIDCall(env, clazz.obj(), id, i);
  base::Time end = base::Time::Now();

  // On a Galaxy Nexus, results were in the range of:
  // JNI LazyMethodIDCall (us) 1984
  // JNI MethodIDCall (us) 1861
  LOG(ERROR) << "JNI LazyMethodIDCall (us) " <<
      base::TimeDelta(end_lazy - start_lazy).InMicroseconds();
  LOG(ERROR) << "JNI MethodIDCall (us) " <<
      base::TimeDelta(end - start).InMicroseconds();
  LOG(ERROR) << "JNI " << o;
}

TEST(JNIAndroidTest, GetJavaStackTraceIfPresent) {
  // The main thread should always have Java frames in it.
  EXPECT_THAT(GetJavaStackTraceIfPresent(), StartsWith("\tat"));

  class HelperThread : public Thread {
   public:
    HelperThread()
        : Thread("TestThread"), java_stack_1_("X"), java_stack_2_("X") {}

    void Init() override {
      // Test without a JNIEnv.
      java_stack_1_ = GetJavaStackTraceIfPresent();

      // Test with a JNIEnv but no Java frames.
      AttachCurrentThread();
      java_stack_2_ = GetJavaStackTraceIfPresent();
    }

    std::string java_stack_1_;
    std::string java_stack_2_;
  };

  HelperThread t;
  t.StartAndWaitForTesting();
  EXPECT_EQ(t.java_stack_1_, "");
  EXPECT_EQ(t.java_stack_2_, "");
}

}  // namespace android
}  // namespace base
