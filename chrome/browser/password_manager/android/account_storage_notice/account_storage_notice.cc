// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_storage_notice/account_storage_notice.h"

#include <utility>

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/password_manager/core/browser/password_store/split_stores_and_local_upm.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SettingsLauncherImpl_jni.h"
#include "chrome/browser/password_manager/android/account_storage_notice/jni/AccountStorageNoticeCoordinator_jni.h"

using base::android::AttachCurrentThread;

// static
std::unique_ptr<AccountStorageNotice> AccountStorageNotice::MaybeShow(
    syncer::SyncService* sync_service,
    PrefService* pref_service,
    ui::WindowAndroid* window_android,
    base::OnceClosure done_cb) {
  base::android::ScopedJavaLocalRef<jobject> java_coordinator =
      Java_AccountStorageNoticeCoordinator_create(
          AttachCurrentThread(),
          sync_service ? sync_service->HasSyncConsent() : false,
          password_manager::sync_util::HasChosenToSyncPasswords(sync_service),
          password_manager::IsGmsCoreUpdateRequired(
              pref_service, sync_service,
              base::android::BuildInfo::GetInstance()->gms_version_code()),
          pref_service->GetJavaObject(), window_android->GetJavaObject(),
          Java_SettingsLauncherImpl_create(AttachCurrentThread()));
  if (java_coordinator) {
    return base::WrapUnique(
        new AccountStorageNotice(java_coordinator, std::move(done_cb)));
  }
  // Creation failed, reply immediately.
  std::move(done_cb).Run();
  return nullptr;
}

AccountStorageNotice::AccountStorageNotice(
    base::android::ScopedJavaLocalRef<jobject> java_coordinator,
    base::OnceClosure done_cb)
    : java_coordinator_(java_coordinator), done_cb_(std::move(done_cb)) {
  CHECK(java_coordinator_);
  CHECK(done_cb_);
  Java_AccountStorageNoticeCoordinator_setObserver(
      AttachCurrentThread(), java_coordinator_,
      reinterpret_cast<intptr_t>(this));
}

AccountStorageNotice::~AccountStorageNotice() {
  // Remove the observer *before* calling hideImmediatelyIfShowing(), because we
  // don't want to trigger OnClosed() and `done_cb` if the sheet was still
  // showing.
  Java_AccountStorageNoticeCoordinator_setObserver(AttachCurrentThread(),
                                                   java_coordinator_, 0);
  Java_AccountStorageNoticeCoordinator_hideImmediatelyIfShowing(
      AttachCurrentThread(), java_coordinator_);
}

void AccountStorageNotice::OnClosed(JNIEnv* env) {
  std::move(done_cb_).Run();
  // `done_cb_` might have deleted the object above, do nothing else.
}
