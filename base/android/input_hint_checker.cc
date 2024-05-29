// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/input_hint_checker.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/InputHintChecker_jni.h"

namespace base::android {

namespace {

bool g_input_hint_enabled;
base::TimeDelta g_poll_interval;
InputHintChecker* g_test_instance;

}  // namespace

// Whether to fetch the input hint from the system. When disabled, pretends
// that no input is ever queued.
BASE_EXPORT
BASE_FEATURE(kYieldWithInputHint,
             "YieldWithInputHint",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Min time delta between checks for the input hint. Must be a smaller than
// time to produce a frame, but a bit longer than the time it takes to retrieve
// the hint.
const base::FeatureParam<int> kPollIntervalMillisParam{&kYieldWithInputHint,
                                                       "poll_interval_ms", 3};

// static
void InputHintChecker::InitializeFeatures() {
  bool is_enabled = base::FeatureList::IsEnabled(kYieldWithInputHint);
  g_input_hint_enabled = is_enabled;
  if (is_enabled) {
    g_poll_interval = Milliseconds(kPollIntervalMillisParam.Get());
  }
}

bool InputHintChecker::HasInputImplWithThrottling() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto now = base::TimeTicks::Now();
  if (last_checked_.is_null() || (now - last_checked_) >= g_poll_interval) {
    last_checked_ = now;
    // TODO(pasko): Implement fetching the hint from the system.
    return false;
  }
  return false;
}

void InputHintChecker::SetView(JNIEnv* env, jobject root_view) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  view_ = JavaObjectWeakGlobalRef(env, root_view);
}

// static
bool InputHintChecker::HasInput() {
  if (!g_input_hint_enabled) {
    return false;
  }
  return GetInstance().HasInputImplWithThrottling();
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
  g_test_instance = nullptr;
}

void JNI_InputHintChecker_SetView(_JNIEnv* env,
                                  const JavaParamRef<jobject>& v) {
  InputHintChecker::GetInstance().SetView(env, v.obj());
}

}  // namespace base::android
