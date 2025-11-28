// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/no_destructor.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_model_observer_helper.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/picture_in_picture/test/jni_headers/AutoPiPTabModelObserverHelperTestUtils_jni.h"

namespace {

struct TestState {
  std::unique_ptr<AutoPictureInPictureTabModelObserverHelper> helper;
  base::android::ScopedJavaGlobalRef<jobject> on_activated_changed_callback;
};

TestState* g_test_state = nullptr;

void RunActivationChangedCallback(bool is_activated) {
  if (g_test_state) {
    base::android::RunBooleanCallbackAndroid(
        g_test_state->on_activated_changed_callback, is_activated);
  }
}

}  // namespace

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

namespace picture_in_picture {

// static
static void JNI_AutoPiPTabModelObserverHelperTestUtils_Initialize(
    JNIEnv* env,
    content::WebContents* web_contents,
    const base::android::JavaParamRef<jobject>& j_callback) {
  // Ensure no previous test state is leaking.
  CHECK(!g_test_state);

  g_test_state = new TestState();
  g_test_state->on_activated_changed_callback.Reset(j_callback);

  g_test_state->helper =
      std::make_unique<AutoPictureInPictureTabModelObserverHelper>(
          web_contents, base::BindRepeating(&RunActivationChangedCallback));
}

// static
static void JNI_AutoPiPTabModelObserverHelperTestUtils_StartObserving(
    JNIEnv* env) {
  CHECK(g_test_state);
  g_test_state->helper->StartObserving();
}

// static
static void JNI_AutoPiPTabModelObserverHelperTestUtils_StopObserving(
    JNIEnv* env) {
  CHECK(g_test_state);
  g_test_state->helper->StopObserving();
}

// static
static void JNI_AutoPiPTabModelObserverHelperTestUtils_Destroy(JNIEnv* env) {
  if (g_test_state) {
    delete g_test_state;
    g_test_state = nullptr;
  }
}

}  // namespace picture_in_picture

DEFINE_JNI(AutoPiPTabModelObserverHelperTestUtils)
