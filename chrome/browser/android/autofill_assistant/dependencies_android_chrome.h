// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_ANDROID_CHROME_H_
#define CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_ANDROID_CHROME_H_

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/autofill_assistant/platform_dependencies_android.h"
#include "chrome/browser/autofill_assistant/common_dependencies_chrome.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/autofill_assistant/browser/android/dependencies_android.h"

namespace autofill_assistant {

// Chrome implementation of DependenciesAndroid.
class DependenciesAndroidChrome : public DependenciesAndroid {
 public:
  DependenciesAndroidChrome(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jstatic_dependencies);

  const CommonDependencies* GetCommonDependencies() const override;
  const PlatformDependencies* GetPlatformDependencies() const override;

 private:
  // Default initialized.
  CommonDependenciesChrome common_dependencies_{
      ProfileManager::GetLastUsedProfile()};
  PlatformDependenciesAndroid platform_dependencies_;
};

}  // namespace autofill_assistant

#endif  // CHROME_BROWSER_ANDROID_AUTOFILL_ASSISTANT_DEPENDENCIES_ANDROID_CHROME_H_
