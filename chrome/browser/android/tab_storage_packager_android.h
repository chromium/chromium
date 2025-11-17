// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_STORAGE_PACKAGER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_TAB_STORAGE_PACKAGER_ANDROID_H_

#include <jni.h>

#include <memory>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab/android_tab_package.h"
#include "chrome/browser/tab/payload.h"
#include "chrome/browser/tab/tab_storage_packager.h"
#include "components/tabs/public/tab_interface.h"

namespace tabs {
class StoragePackage;
class TabInterface;

// The types of TabModels, differing in terms of scope.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.app.tabmodel
enum class TabModelType {
  kUnknown = 0,
  // Scoped to a window ID & regular profile.
  kRegular = 1,
  // Scoped to a window ID & OTR profile.
  kIncognito = 2,
  // Scoped to a profile.
  kArchived = 3,
  kMaxValue = kArchived,
};

// This class is the Android implementation of the TabStoragePackager.
class TabStoragePackagerAndroid : public TabStoragePackager {
 public:
  explicit TabStoragePackagerAndroid(Profile* profile);
  ~TabStoragePackagerAndroid() override;

  TabStoragePackagerAndroid(const TabStoragePackagerAndroid&) = delete;
  TabStoragePackagerAndroid& operator=(const TabStoragePackagerAndroid&) =
      delete;

  // TabStoragePackager override:
  bool IsOffTheRecord(const TabCollection* collection) const override;
  std::string GetWindowTag(const TabCollection* collection) const override;
  std::unique_ptr<StoragePackage> Package(const TabInterface* tab) override;

  // Returns a pointer to TabStoragePackage (as a long in Java). The caller is
  // responsible for managing the lifecycle of the returned object.
  long ConsolidateTabData(
      JNIEnv* env,
      jlong timestamp_millis,
      const jni_zero::JavaParamRef<jobject>& web_contents_state_buffer,
      std::optional<std::string> opener_app_id,
      jint theme_color,
      jlong last_navigation_committed_timestamp_millis,
      jboolean tab_has_sensitive_content,
      TabAndroid* tab);
  // Returns a pointer to an UnmappedTabStripCollectionStorageData (as a long in
  // Java). The caller is responsible for managing the lifecycle of the returned
  // object.
  long ConsolidateTabStripCollectionData(JNIEnv* env,
                                         jint window_id,
                                         jint j_tab_model_type,
                                         TabAndroid* active_tab);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  std::unique_ptr<Payload> PackageTabStripCollectionData(
      const TabStripCollection* collection,
      StorageIdMapping& mapping) override;

 private:
  // A reference to the Java version of this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  raw_ptr<Profile> profile_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_STORAGE_PACKAGER_ANDROID_H_
