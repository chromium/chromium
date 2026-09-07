// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/pwa_restore_bottom_sheet_mediator.h"

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/notreached.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/pwa_restore_bottom_sheet_mediator_jni_headers/PwaRestoreBottomSheetMediator_jni.h"

using base::android::JavaRef;

namespace webapk {

// static
static int64_t JNI_PwaRestoreBottomSheetMediator_Initialize(
    JNIEnv* env,
    const JavaRef<jobject>& java_ref) {
  // TODO(crbug.com/400662034): Delete this.
  NOTREACHED();
}

PwaRestoreBottomSheetMediator::PwaRestoreBottomSheetMediator(
    const JavaRef<jobject>& java_ref,
    WebApkRestoreManager* restore_manager)
    : restore_manager_(restore_manager->GetWeakPtr()) {
  java_ref_.Reset(java_ref);
}

PwaRestoreBottomSheetMediator::~PwaRestoreBottomSheetMediator() = default;

void PwaRestoreBottomSheetMediator::Destroy(JNIEnv* env) {
  if (restore_manager_) {
    restore_manager_->ResetIfNotRunning();
  }
  delete this;
}

void PwaRestoreBottomSheetMediator::OnRestoreWebapps(
    JNIEnv* env,
    const JavaRef<jobjectArray>& jrestore_app_ids) {
  if (!restore_manager_) {
    return;
  }

  std::vector<std::string> app_ids_to_restore;
  base::android::AppendJavaStringArrayToStringVector(env, jrestore_app_ids,
                                                     &app_ids_to_restore);
  restore_manager_->ScheduleRestoreTasks(app_ids_to_restore);
}

}  // namespace webapk

DEFINE_JNI(PwaRestoreBottomSheetMediator)
