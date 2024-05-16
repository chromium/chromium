// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_storage_notice/account_storage_notice.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/SettingsLauncherImpl_jni.h"
#include "chrome/browser/password_manager/android/account_storage_notice/jni/AccountStorageNoticeCoordinator_jni.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/native_widget_types.h"

using base::android::AttachCurrentThread;

// static
bool AccountStorageNotice::ShouldShow(PrefService* pref_service,
                                      syncer::SyncService* sync_service) {
  // TODO(crbug.com/338576301): Consider checking UPM predicates here too.
  return sync_service && !sync_service->HasSyncConsent() &&
         sync_service->GetUserSettings()->GetSelectedTypes().Has(
             syncer::UserSelectableType::kPasswords) &&
         !pref_service->GetBoolean(
             password_manager::prefs::kAccountStorageNoticeShown) &&
         base::FeatureList::IsEnabled(
             syncer::kEnablePasswordsAccountStorageForNonSyncingUsers);
}

AccountStorageNotice::AccountStorageNotice(content::WebContents* web_contents,
                                           PrefService* pref_service,
                                           syncer::SyncService* sync_service,
                                           base::OnceClosure closed_cb)
    : java_coordinator_(Java_AccountStorageNoticeCoordinator_Constructor(
          AttachCurrentThread(),
          web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
          Java_SettingsLauncherImpl_create(AttachCurrentThread()),
          reinterpret_cast<intptr_t>(this))),
      closed_cb_(std::move(closed_cb)) {
  CHECK(closed_cb_);
  CHECK(ShouldShow(pref_service, sync_service));
  pref_service->SetBoolean(password_manager::prefs::kAccountStorageNoticeShown,
                           true);
}

AccountStorageNotice::~AccountStorageNotice() {
  if (java_coordinator_) {
    // See destructor docs as to when this can happen.
    Java_AccountStorageNoticeCoordinator_destroy(AttachCurrentThread(),
                                                 java_coordinator_);
  }
}

void AccountStorageNotice::OnClosed(JNIEnv* env) {
  CHECK(java_coordinator_);
  Java_AccountStorageNoticeCoordinator_destroy(AttachCurrentThread(),
                                               java_coordinator_);
  java_coordinator_.Reset();
  std::move(closed_cb_).Run();
  // `closed_cb_` might have deleted the object above, do nothing else.
}
