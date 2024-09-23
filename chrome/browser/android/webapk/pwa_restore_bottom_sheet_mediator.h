// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_PWA_RESTORE_BOTTOM_SHEET_MEDIATOR_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_PWA_RESTORE_BOTTOM_SHEET_MEDIATOR_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/webapk/webapk_restore_manager.h"

namespace webapk {

class PwaRestoreBottomSheetMediator {
 public:
  explicit PwaRestoreBottomSheetMediator(
      const base::android::JavaParamRef<jobject>& java_ref,
      WebApkRestoreManager* restore_manager);
  PwaRestoreBottomSheetMediator(const PwaRestoreBottomSheetMediator&) = delete;
  PwaRestoreBottomSheetMediator& operator=(
      const PwaRestoreBottomSheetMediator&) = delete;

  void Destroy(JNIEnv* env);

  void OnRestoreWebapps(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jrestore_app_ids);

 private:
  ~PwaRestoreBottomSheetMediator();

  // Points to the Java reference.
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;

  base::WeakPtr<WebApkRestoreManager> restore_manager_;
};

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_PWA_RESTORE_BOTTOM_SHEET_MEDIATOR_H_
