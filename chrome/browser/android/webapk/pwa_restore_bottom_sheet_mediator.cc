// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/pwa_restore_bottom_sheet_mediator.h"

#include <jni.h>

#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/feature_list.h"
#include "chrome/browser/android/webapk/webapk_sync_service.h"
#include "chrome/browser/android/webapk/webapk_sync_service_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/sync/base/features.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/webapps/browser/android/pwa_restore_bottom_sheet_mediator_jni_headers/PwaRestoreBottomSheetMediator_jni.h"

using base::android::JavaParamRef;

namespace webapk {

// static
jlong JNI_PwaRestoreBottomSheetMediator_Initialize(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_ref) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (profile == nullptr) {
    return 0;
  }

  WebApkRestoreManager* restore_manager =
      WebApkSyncServiceFactory::GetForProfile(profile)
          ->GetWebApkRestoreManager();

  return reinterpret_cast<intptr_t>(
      new PwaRestoreBottomSheetMediator(java_ref, restore_manager));
}

PwaRestoreBottomSheetMediator::PwaRestoreBottomSheetMediator(
    const JavaParamRef<jobject>& java_ref,
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
    const JavaParamRef<jobjectArray>& jrestore_app_ids) {
  if (!restore_manager_) {
    return;
  }

  std::vector<std::string> app_ids_to_restore;
  base::android::AppendJavaStringArrayToStringVector(env, jrestore_app_ids,
                                                     &app_ids_to_restore);
  restore_manager_->ScheduleRestoreTasks(app_ids_to_restore);
}

}  // namespace webapk
