// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/input_hint_checker.h"

#include <jni.h>
#include <pthread.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/rand_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/InputHintChecker_jni.h"

namespace base::android {

enum class InputHintChecker::InitState {
  kNotStarted,
  kInProgress,
  kInitialized,
  kFailedToInitialize
};

namespace {

bool g_input_hint_enabled;
base::TimeDelta g_poll_interval;
InputHintChecker* g_test_instance;

}  // namespace

// Whether to fetch the input hint from the system. When disabled, pretends
// that no input is ever queued.
BASE_FEATURE(kYieldWithInputHint, base::FEATURE_ENABLED_BY_DEFAULT);

// Min time delta between checks for the input hint. Must be a smaller than
// time to produce a frame, but a bit longer than the time it takes to retrieve
// the hint.
// Note: Do not use the prepared macro as of no need for a local cache.
const base::FeatureParam<int> kPollIntervalMillisParam{&kYieldWithInputHint,
                                                       "poll_interval_ms", 1};

// Class calling a private method of InputHintChecker.
// This allows not to declare the method called by pthread_create in the public
// header.
class InputHintChecker::OffThreadInitInvoker {
 public:
  // Called by pthread_create().
  static void* Run(void* opaque) {
    InputHintChecker::GetInstance().RunOffThreadInitialization();
    return nullptr;
  }
};

InputHintChecker::InputHintChecker() : init_state_(InitState::kNotStarted) {}

InputHintChecker::~InputHintChecker() {
  // If off-thread initialization via pthread_create is still in progress, wait
  // for it to complete before destroying members (e.g. `view_class_`) that the
  // background thread accesses.
  while (FetchState() == InitState::kInProgress) {
    PlatformThread::YieldCurrentThread();
  }
}

// static
void InputHintChecker::InitializeFeatures() {
  bool is_enabled = base::FeatureList::IsEnabled(kYieldWithInputHint);
  g_input_hint_enabled = is_enabled;
  if (is_enabled) {
    g_poll_interval = Milliseconds(kPollIntervalMillisParam.Get());
  }
}

void InputHintChecker::SetView(JNIEnv* env,
                               const jni_zero::JavaRef<jobject>& root_view) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  InitState state = FetchState();
  if (state == InitState::kFailedToInitialize) {
    return;
  }
  view_ = JavaObjectWeakGlobalRef(env, root_view);
  if (!root_view) {
    return;
  }
  if (state == InitState::kNotStarted) {
    // Store the View.class and continue initialization on another thread. A
    // separate non-Java thread is required to obtain a reference to
    // j.l.reflect.Method via double-reflection.
    TransitionToState(InitState::kInProgress);
    view_class_ = ScopedJavaGlobalRef<jobject>(
        env, jni_zero::AdoptRef(env, env->GetObjectClass(root_view.obj())));
    pthread_t new_thread;
    if (pthread_create(&new_thread, nullptr, OffThreadInitInvoker::Run,
                       nullptr) != 0) {
      PLOG(ERROR) << "pthread_create";
      TransitionToState(InitState::kFailedToInitialize);
    }
  }
}

// static
bool InputHintChecker::HasInput() {
  if (!g_input_hint_enabled) {
    return false;
  }
  return GetInstance().HasInputImplWithThrottling();
}

bool InputHintChecker::IsInitializedForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return FetchState() == InitState::kInitialized;
}

bool InputHintChecker::FailedToInitializeForTesting() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return FetchState() == InitState::kFailedToInitialize;
}

bool InputHintChecker::HasInputImplWithThrottling() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Early return if off-thread initialization has not succeeded yet.
  InitState state = FetchState();
  if (state != InitState::kInitialized) {
    return false;
  }

  // Input processing is associated with the root view. Early return when the
  // root view is not available. It can happen in cases like multi-window.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> scoped_view = view_.get(env);
  if (!scoped_view) {
    return false;
  }

  // Throttle.
  auto now = base::TimeTicks::Now();
  if (last_checked_.is_null() || (now - last_checked_) >= g_poll_interval) {
    last_checked_ = now;
  } else {
    return false;
  }

  return HasInputImpl(env, scoped_view.obj());
}

bool InputHintChecker::HasInputImplNoThrottlingForTesting(_JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (FetchState() != InitState::kInitialized) {
    return false;
  }
  ScopedJavaLocalRef<jobject> scoped_view = view_.get(env);
  CHECK(scoped_view.obj());
  return HasInputImpl(env, scoped_view.obj());
}

bool InputHintChecker::HasInputImplWithThrottlingForTesting(_JNIEnv* env) {
  if (FetchState() != InitState::kInitialized) {
    return false;
  }
  return HasInputImplWithThrottling();
}

bool InputHintChecker::HasInputImpl(JNIEnv* env, jobject o) {
  bool value =
      static_cast<bool>(env->CallBooleanMethod(o, has_input_method_id_));
  if (ClearException(env)) {
    LOG(ERROR) << "Exception when calling has_input_method_id_";
    TransitionToState(InitState::kFailedToInitialize);
    return false;
  }
  return value;
}

InputHintChecker::InitState InputHintChecker::FetchState() const {
  return init_state_.load(std::memory_order_acquire);
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class InitializationResult {
  kSuccess = 0,
  kFailure = 1,
  kMaxValue = kFailure,
};

void InputHintChecker::TransitionToState(InitState new_state) {
  DCHECK_NE(new_state, FetchState());
  if (new_state == InitState::kInitialized ||
      new_state == InitState::kFailedToInitialize) {
    InitializationResult r = (new_state == InitState::kInitialized)
                                 ? InitializationResult::kSuccess
                                 : InitializationResult::kFailure;
    UmaHistogramEnumeration("Android.InputHintChecker.InitializationResult", r);
  }
  init_state_.store(new_state, std::memory_order_release);
}

void InputHintChecker::RunOffThreadInitialization() {
  JNIEnv* env = AttachCurrentThread();
  InitGlobalRefsAndMethodIds(env);
  DetachFromVM();
}

void InputHintChecker::InitGlobalRefsAndMethodIds(JNIEnv* env) {
  // Obtain j.l.reflect.Method using View.class.getMethod("probablyHasInput",
  // "...").
  jclass view_class = env->GetObjectClass(view_class_.obj());
  if (ClearException(env)) {
    LOG(ERROR) << "exception on GetObjectClass(view)";
    TransitionToState(InitState::kFailedToInitialize);
    return;
  }
  jmethodID get_method_id = env->GetMethodID(
      view_class, "getMethod",
      "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;");
  if (ClearException(env)) {
    LOG(ERROR) << "exception when looking for method getMethod()";
    TransitionToState(InitState::kFailedToInitialize);
    return;
  }
  ScopedJavaLocalRef<jstring> has_input_string =
      jni_zero::NewAsciiString(env, "probablyHasInput");
  ScopedJavaLocalRef<jobject> method = jni_zero::AdoptRef(
      env, env->CallObjectMethod(view_class_.obj(), get_method_id,
                                 has_input_string.obj(), nullptr));
  if (ClearException(env)) {
    LOG(ERROR) << "exception when calling getMethod(probablyHasInput)";
    TransitionToState(InitState::kFailedToInitialize);
    return;
  }
  if (!method) {
    LOG(ERROR) << "got null from getMethod(probablyHasInput)";
    TransitionToState(InitState::kFailedToInitialize);
    return;
  }

  has_input_method_id_ = env->FromReflectedMethod(method.obj());
  if (ClearException(env) || !has_input_method_id_) {
    LOG(ERROR) << "exception on FromReflectedMethod or null returned";
    TransitionToState(InitState::kFailedToInitialize);
    return;
  }

  // Publish the obtained members to the thread observing kInitialized.
  TransitionToState(InitState::kInitialized);
}

InputHintChecker& InputHintChecker::GetInstance() {
  static NoDestructor<InputHintChecker> checker;
  if (g_test_instance) {
    return *g_test_instance;
  }
  return *checker.get();
}

InputHintChecker::ScopedOverrideInstance::ScopedOverrideInstance(
    InputHintChecker* checker) {
  g_test_instance = checker;
}

InputHintChecker::ScopedOverrideInstance::~ScopedOverrideInstance() {
  // OffThreadInitInvoker::Run() calls
  // InputHintChecker::GetInstance().RunOffThreadInitialization(), which
  // resolves to g_test_instance. The override must remain installed until any
  // in-flight off-thread initialization finishes so that the background thread
  // does not access an unexpected instance.
  if (g_test_instance) {
    while (g_test_instance->FetchState() == InitState::kInProgress) {
      PlatformThread::YieldCurrentThread();
    }
  }
  g_test_instance = nullptr;
}

void InputHintChecker::RecordInputHintResult(InputHintResult result) {
  if (!metric_subsampling_disabled_ &&
      !base::ShouldRecordSubsampledMetric(0.001)) {
    return;
  }
  UMA_HISTOGRAM_ENUMERATION("Android.InputHintChecker.InputHintResult", result);
}

static void JNI_InputHintChecker_SetView(_JNIEnv* env,
                                         const jni_zero::JavaRef<jobject>& v) {
  InputHintChecker::GetInstance().SetView(env, v);
}

static void JNI_InputHintChecker_OnCompositorViewHolderTouchEvent(
    _JNIEnv* env) {
  auto& checker = InputHintChecker::GetInstance();
  if (checker.is_after_input_yield()) {
    checker.RecordInputHintResult(InputHintResult::kCompositorViewTouchEvent);
  }
  checker.set_is_after_input_yield(false);
}

static bool JNI_InputHintChecker_IsInitializedForTesting(_JNIEnv* env) {
  return InputHintChecker::GetInstance().IsInitializedForTesting();  // IN-TEST
}

static bool JNI_InputHintChecker_FailedToInitializeForTesting(_JNIEnv* env) {
  return InputHintChecker::GetInstance()
      .FailedToInitializeForTesting();  // IN-TEST
}

static bool JNI_InputHintChecker_HasInputForTesting(_JNIEnv* env) {
  InputHintChecker& checker = InputHintChecker::GetInstance();
  return checker.HasInputImplNoThrottlingForTesting(env);  // IN-TEST
}

static bool JNI_InputHintChecker_HasInputWithThrottlingForTesting(
    _JNIEnv* env) {
  InputHintChecker& checker = InputHintChecker::GetInstance();
  return checker.HasInputImplWithThrottlingForTesting(env);  // IN-TEST
}

static void JNI_InputHintChecker_SetIsAfterInputYieldForTesting(  // IN-TEST
    _JNIEnv* env,
    bool after) {
  InputHintChecker::GetInstance().disable_metric_subsampling();
  InputHintChecker::GetInstance().set_is_after_input_yield(after);
}

}  // namespace base::android

DEFINE_JNI(InputHintChecker)
