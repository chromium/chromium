// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/android/first_run_prefs.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/FirstRunUtils_jni.h"

static jboolean JNI_FirstRunUtils_GetFirstRunEulaAccepted(JNIEnv* env) {
  return g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

static void JNI_FirstRunUtils_SetEulaAccepted(JNIEnv* env) {
  g_browser_process->local_state()->SetBoolean(prefs::kEulaAccepted, true);
}

static jboolean JNI_FirstRunUtils_GetCctTosDialogEnabled(JNIEnv* env) {
  int behavior = g_browser_process->local_state()->GetInteger(
      first_run::kTosDialogBehavior);
  return static_cast<first_run::TosDialogBehavior>(behavior) !=
         first_run::TosDialogBehavior::SKIP;
}
