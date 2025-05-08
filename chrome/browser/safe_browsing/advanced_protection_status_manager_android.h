// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_ANDROID_H_

#include "base/android/jni_android.h"
#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

namespace safe_browsing {

// Android implementation of `AdvancedProtectionStatusManager`.
class AdvancedProtectionStatusManagerAndroid
    : public AdvancedProtectionStatusManager {
 public:
  AdvancedProtectionStatusManagerAndroid();
  ~AdvancedProtectionStatusManagerAndroid() override;

  AdvancedProtectionStatusManagerAndroid(
      const AdvancedProtectionStatusManagerAndroid&) = delete;
  AdvancedProtectionStatusManagerAndroid& operator=(
      const AdvancedProtectionStatusManagerAndroid&) = delete;

  static bool QueryIsUnderAdvancedProtection();

  // AdvancedProtectionStatusManager:
  bool IsUnderAdvancedProtection() const override;
  void SetAdvancedProtectionStatusForTesting(bool enrolled) override;

  void OnAdvancedProtectionOsSettingChanged(JNIEnv* env);

 private:
  void UpdateState();

  base::android::ScopedJavaGlobalRef<jobject> java_manager_;
  bool is_under_advanced_protection_ = false;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_ANDROID_H_
