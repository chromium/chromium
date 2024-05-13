// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/account_storage_notice/account_storage_notice.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/check.h"
#include "chrome/android/chrome_jni_headers/SettingsLauncherImpl_jni.h"
#include "chrome/browser/password_manager/android/account_storage_notice/jni/AccountStorageNoticeCoordinator_jni.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/native_widget_types.h"

using base::android::AttachCurrentThread;

AccountStorageNotice::AccountStorageNotice(content::WebContents* web_contents,
                                           base::OnceClosure accepted_cb)
    : java_coordinator_(Java_AccountStorageNoticeCoordinator_Constructor(
          AttachCurrentThread(),
          web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject(),
          Java_SettingsLauncherImpl_create(AttachCurrentThread()),
          reinterpret_cast<intptr_t>(this))),
      accepted_cb_(std::move(accepted_cb)) {
  CHECK(accepted_cb_);
}

AccountStorageNotice::~AccountStorageNotice() {
  if (java_coordinator_) {
    // See destructor docs as to when this can happen.
    Java_AccountStorageNoticeCoordinator_destroy(AttachCurrentThread(),
                                                 java_coordinator_);
  }
}

void AccountStorageNotice::OnAccepted(JNIEnv* env) {
  CHECK(java_coordinator_);
  Java_AccountStorageNoticeCoordinator_destroy(AttachCurrentThread(),
                                               java_coordinator_);
  java_coordinator_.Reset();
  std::move(accepted_cb_).Run();
  // `accepted_cb_` might have deleted the object above, do nothing else.
}
