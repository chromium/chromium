// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_ADVANCED_PROTECTION_STATUS_MANAGER_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_ADVANCED_PROTECTION_STATUS_MANAGER_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"

namespace android_webview {

// Native implementation of AwAdvancedProtectionStatusManagerBridge.
class AwAdvancedProtectionStatusManagerBridge {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAdvancedProtectionStatusChanged(
        bool is_under_advanced_protection) = 0;
  };

  static AwAdvancedProtectionStatusManagerBridge* GetInstance();

  AwAdvancedProtectionStatusManagerBridge(
      const AwAdvancedProtectionStatusManagerBridge&) = delete;
  AwAdvancedProtectionStatusManagerBridge& operator=(
      const AwAdvancedProtectionStatusManagerBridge&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called when the Android-OS advanced-protection-mode setting changes.
  void OnAdvancedProtectionOsSettingChanged();

  // Static helper to query the Java side for the advanced protection status.
  static bool IsUnderAdvancedProtection();

 private:
  friend class base::NoDestructor<AwAdvancedProtectionStatusManagerBridge>;

  AwAdvancedProtectionStatusManagerBridge();
  ~AwAdvancedProtectionStatusManagerBridge();

  base::ObserverList<Observer> observers_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_ADVANCED_PROTECTION_STATUS_MANAGER_BRIDGE_H_
