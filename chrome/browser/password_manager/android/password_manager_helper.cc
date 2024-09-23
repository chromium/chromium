// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/sync/android/sync_service_android_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/password_manager/android/jni_headers/PasswordManagerHelper_jni.h"

jboolean JNI_PasswordManagerHelper_HasChosenToSyncPasswords(
    JNIEnv* env,
    syncer::SyncService* sync_service) {
  return password_manager::sync_util::HasChosenToSyncPasswords(sync_service);
}
