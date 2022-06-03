// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lifetime/application_lifetime_android.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "chrome/android/chrome_jni_headers/ApplicationLifetime_jni.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace chrome {

void AttemptRestart() {
  // Set the flag to restart Chrome after it is shutdown.
  PrefService* pref_service = g_browser_process->local_state();
  pref_service->SetBoolean(prefs::kRestartLastSessionOnShutdown, true);
  AttemptExit();
}


void TerminateAndroid() {
  bool restart = false;
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown)) {
    restart = prefs->GetBoolean(prefs::kRestartLastSessionOnShutdown);
    prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ApplicationLifetime_terminate(env, restart);
}

}  // namespace chrome
