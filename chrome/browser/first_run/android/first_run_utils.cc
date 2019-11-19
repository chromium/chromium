// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/android/chrome_jni_headers/FirstRunUtils_jni.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_service.h"
#include "components/web_resource/web_resource_pref_names.h"

static jboolean JNI_FirstRunUtils_GetFirstRunEulaAccepted(JNIEnv* env) {
  return g_browser_process->local_state()->GetBoolean(prefs::kEulaAccepted);
}

static void JNI_FirstRunUtils_SetEulaAccepted(JNIEnv* env) {
  g_browser_process->local_state()->SetBoolean(prefs::kEulaAccepted, true);
}
