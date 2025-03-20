// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_ANDROID_H_

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

  // AdvancedProtectionStatusManager:
  bool IsUnderAdvancedProtection() const override;
  void AddObserver(StatusChangedObserver* observer) override;
  void RemoveObserver(StatusChangedObserver* observer) override;
  void SetAdvancedProtectionStatusForTesting(bool enrolled) override;

 private:
  bool is_under_advanced_protection_ = false;
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_ADVANCED_PROTECTION_STATUS_MANAGER_ANDROID_H_
