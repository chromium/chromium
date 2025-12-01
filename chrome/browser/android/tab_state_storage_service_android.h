// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_STATE_STORAGE_SERVICE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_STATE_STORAGE_SERVICE_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/tab/tab_state_storage_backend.h"
#include "chrome/browser/tab/tab_state_storage_service.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/keyed_service/core/keyed_service.h"

namespace tabs {

const char kTabStateStorageServiceAndroidKey[] =
    "tab_state_storage_service_android";

class TabStateStorageServiceAndroid : public base::SupportsUserData::Data {
 public:
  explicit TabStateStorageServiceAndroid(
      TabStateStorageService* tab_state_storage_service);
  ~TabStateStorageServiceAndroid() override;

  void BoostPriority(JNIEnv* env);

  void Save(JNIEnv* env, TabAndroid* tab);

  void LoadAllData(
      JNIEnv* env,
      const std::string& window_tag,
      bool is_off_the_record,
      const jni_zero::JavaParamRef<jobject>& j_loaded_data_callback);

  void ClearState(JNIEnv* env);

  void ClearWindow(JNIEnv* env, const std::string& window_tag);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // A reference to the Java version of this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  // Not owned by this class.
  raw_ptr<TabStateStorageService> tab_state_storage_service_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_STATE_STORAGE_SERVICE_ANDROID_H_
