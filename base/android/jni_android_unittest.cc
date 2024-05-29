// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"

#include <optional>
#include <string>

#include "base/android/java_exception_reporter.h"
#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_unittest_support_jni/JniAndroidTestUtils_jni.h"

using ::testing::Eq;
using ::testing::Optional;
using ::testing::StartsWith;

namespace base {
namespace android {

namespace {

class JniAndroidExceptionTestContext final {
 public:
  JniAndroidExceptionTestContext() {
    CHECK(instance == nullptr);
    instance = this;
    SetJavaExceptionCallback(CapturingExceptionCallback);
    g_log_fatal_callback_for_testing = CapturingLogFatalCallback;
  }

  ~JniAndroidExceptionTestContext() {
    g_log_fatal_callback_for_testing = nullptr;
    SetJavaExceptionCallback(prev_exception_callback_);
    env->ExceptionClear();
    Java_JniAndroidTestUtils_restoreGlobalExceptionHandler(env);
    instance = nullptr;
  }

 private:
  static void CapturingLogFatalCallback(const char* message) {
    auto* self = instance;
    CHECK(self);
    // Capture only the first one (can be called multiple times due to
    // LOG(FATAL) not terminating).
    if (!self->assertion_message) {
      self->assertion_message = message;
    }
  }

  static void CapturingExceptionCallback(const char* message) {
    auto* self = instance;
    CHECK(self);
    if (self->throw_in_exception_callback) {
      self->throw_in_exception_callback = false;
      Java_JniAndroidTestUtils_throwRuntimeException(self->env);
    } else if (self->throw_oom_in_exception_callback) {
      self->throw_oom_in_exception_callback = false;
      Java_JniAndroidTestUtils_throwOutOfMemoryError(self->env);
    } else {
      self->last_java_exception = message;
    }
  }

  static JniAndroidExceptionTestContext* instance;
  const JavaExceptionCallback prev_exception_callback_ =
      GetJavaExceptionCallback();

 public:
  const raw_ptr<JNIEnv> env = base::android::AttachCurrentThread();
  bool throw_in_exception_callback = false;
  bool throw_oom_in_exception_callback = false;
  std::optional<std::string> assertion_message;
  std::optional<std::string> last_java_exception;
};

JniAndroidExceptionTestContext* JniAndroidExceptionTestContext::instance =
    nullptr;

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

TEST(JniAndroidTest, GetJavaStackTraceIfPresent_Normal) {
  // The main thread should always have Java frames in it.
  EXPECT_THAT(GetJavaStackTraceIfPresent(), StartsWith("\tat"));
}

TEST(JniAndroidTest, GetJavaStackTraceIfPresent_NoEnv) {
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

TEST(JniAndroidTest, GetJavaStackTraceIfPresent_PendingException) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JniAndroidTestUtils_throwRuntimeExceptionUnchecked(env);
  std::string result = GetJavaStackTraceIfPresent();
  env->ExceptionClear();
  EXPECT_EQ(result, kUnableToGetStackTraceMessage);
}

TEST(JniAndroidTest, GetJavaStackTraceIfPresent_OutOfMemoryError) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JniAndroidTestUtils_setSimulateOomInSanitizedStacktrace(env, true);
  std::string result = GetJavaStackTraceIfPresent();
  Java_JniAndroidTestUtils_setSimulateOomInSanitizedStacktrace(env, false);
  EXPECT_EQ(result, "");
}

TEST(JniAndroidExceptionTest, HandleExceptionInNative) {
  JniAndroidExceptionTestContext ctx;
  test::ScopedFeatureList feature_list;
  feature_list.InitFromCommandLine("", "HandleJniExceptionsInJava");

  // Do not call setGlobalExceptionHandlerAsNoOp().

  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);
  EXPECT_THAT(ctx.last_java_exception,
              Optional(StartsWith("java.lang.RuntimeException")));
  EXPECT_THAT(ctx.assertion_message, Optional(Eq(kUncaughtExceptionMessage)));
}

TEST(JniAndroidExceptionTest, HandleExceptionInJava_NoOpHandler) {
  JniAndroidExceptionTestContext ctx;
  Java_JniAndroidTestUtils_setGlobalExceptionHandlerAsNoOp(ctx.env);
  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);

  EXPECT_THAT(ctx.last_java_exception,
              Optional(StartsWith("java.lang.RuntimeException")));
  EXPECT_THAT(ctx.assertion_message,
              Optional(Eq(kUncaughtExceptionHandlerFailedMessage)));
}

TEST(JniAndroidExceptionTest, HandleExceptionInJava_ThrowingHandler) {
  JniAndroidExceptionTestContext ctx;
  Java_JniAndroidTestUtils_setGlobalExceptionHandlerToThrow(ctx.env);
  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);

  EXPECT_THAT(ctx.last_java_exception,
              Optional(StartsWith("java.lang.IllegalStateException")));
  EXPECT_THAT(ctx.assertion_message,
              Optional(Eq(kUncaughtExceptionHandlerFailedMessage)));
}

TEST(JniAndroidExceptionTest, HandleExceptionInJava_OomThrowingHandler) {
  JniAndroidExceptionTestContext ctx;
  Java_JniAndroidTestUtils_setGlobalExceptionHandlerToThrowOom(ctx.env);
  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);

  // Should still report the original exception when the global exception
  // handler throws an OutOfMemoryError.
  EXPECT_THAT(ctx.last_java_exception,
              Optional(StartsWith("java.lang.RuntimeException")));
  EXPECT_THAT(ctx.assertion_message,
              Optional(Eq(kUncaughtExceptionHandlerFailedMessage)));
}

TEST(JniAndroidExceptionTest, HandleExceptionInJava_OomInGetJavaExceptionInfo) {
  JniAndroidExceptionTestContext ctx;
  Java_JniAndroidTestUtils_setGlobalExceptionHandlerToThrowOom(ctx.env);
  Java_JniAndroidTestUtils_setSimulateOomInSanitizedStacktrace(ctx.env, true);
  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);
  Java_JniAndroidTestUtils_setSimulateOomInSanitizedStacktrace(ctx.env, false);

  EXPECT_THAT(ctx.last_java_exception,
              Optional(Eq(kOomInGetJavaExceptionInfoMessage)));
  EXPECT_THAT(ctx.assertion_message,
              Optional(Eq(kUncaughtExceptionHandlerFailedMessage)));
}

TEST(JniAndroidExceptionTest, HandleExceptionInJava_Reentrant) {
  JniAndroidExceptionTestContext ctx;
  // Use the SetJavaException() callback to trigger re-entrancy.
  Java_JniAndroidTestUtils_setGlobalExceptionHandlerToThrow(ctx.env);
  ctx.throw_in_exception_callback = true;
  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);

  EXPECT_THAT(ctx.last_java_exception, Optional(Eq(kReetrantExceptionMessage)));
  EXPECT_THAT(ctx.assertion_message, Optional(Eq(kReetrantExceptionMessage)));
}

TEST(JniAndroidExceptionTest, HandleExceptionInJava_ReentrantOom) {
  JniAndroidExceptionTestContext ctx;
  // Use the SetJavaException() callback to trigger re-entrancy.
  Java_JniAndroidTestUtils_setGlobalExceptionHandlerToThrow(ctx.env);
  ctx.throw_oom_in_exception_callback = true;
  Java_JniAndroidTestUtils_throwRuntimeException(ctx.env);

  EXPECT_THAT(ctx.last_java_exception,
              Optional(Eq(kReetrantOutOfMemoryMessage)));
  EXPECT_THAT(ctx.assertion_message, Optional(Eq(kReetrantOutOfMemoryMessage)));
}

}  // namespace android
}  // namespace base
