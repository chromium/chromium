// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/android/pref_service_android.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ChromeBackupAgentImpl_jni.h"

static void JNI_ChromeBackupAgentImpl_CommitPendingPrefWrites(
    JNIEnv* env,
    PrefService* pref_service) {
  // TODO(crbug.com/332710541): This currently doesn't wait for the commit to
  // complete (it passes the default value for the reply_callback param). Wait
  // for the commit to complete, here or in Java.
  pref_service->CommitPendingWrite();
}

DEFINE_JNI(ChromeBackupAgentImpl)
