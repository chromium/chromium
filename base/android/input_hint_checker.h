// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_INPUT_HINT_CHECKER_H_
#define BASE_ANDROID_INPUT_HINT_CHECKER_H_

#include "base/android/jni_weak_ref.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"

namespace base::android {

BASE_DECLARE_FEATURE(kYieldWithInputHint);

// A class to track a single global root View object and ask it for presence of
// new unhandled input events.
//
// This class uses bits specific to Android V and does nothing on earlier
// releases.
//
// Must be constructed on UI thread. All public methods must be called on the UI
// thread.
class BASE_EXPORT InputHintChecker {
 public:
  InputHintChecker() = default;
  virtual ~InputHintChecker() = default;

  // Returns the singleton.
  static InputHintChecker& GetInstance();

  // Initializes features for this class. See `base::features::Init()`.
  static void InitializeFeatures();

  // Obtains a weak reference to |root_view| so that the following calls to
  // HasInput() take the input hint for this View. Requirements for the View
  // object are described in InputHintChecker.java.
  void SetView(JNIEnv* env, jobject root_view);

  // Fetches and returns the input hint from the Android Framework. Throttles
  // the calls to one every few milliseconds. When a call is made before the
  // minimal time interval passed since the previous call, returns false.
  static bool HasInput();

  // RAII override of GetInstance() for testing.
  struct ScopedOverrideInstance {
    explicit ScopedOverrideInstance(InputHintChecker* checker);
    ~ScopedOverrideInstance();
  };

 protected:
  virtual bool HasInputImplWithThrottling();

 private:
  friend class base::NoDestructor<InputHintChecker>;

  JavaObjectWeakGlobalRef view_;
  THREAD_CHECKER(thread_checker_);
  base::TimeTicks last_checked_;
};

}  // namespace base::android

#endif  // BASE_ANDROID_INPUT_HINT_CHECKER_H_
