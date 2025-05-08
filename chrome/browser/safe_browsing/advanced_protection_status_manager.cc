// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager.h"

namespace safe_browsing {

AdvancedProtectionStatusManager::AdvancedProtectionStatusManager() = default;

void AdvancedProtectionStatusManager::AddObserver(
    StatusChangedObserver* observer) {
  observers_.AddObserver(observer);
}

void AdvancedProtectionStatusManager::RemoveObserver(
    StatusChangedObserver* observer) {
  observers_.RemoveObserver(observer);
}

AdvancedProtectionStatusManager::~AdvancedProtectionStatusManager() = default;

void AdvancedProtectionStatusManager::NotifyObserversStatusChanged() {
  bool is_under_advanced_protection = IsUnderAdvancedProtection();
  for (StatusChangedObserver& observer : observers_) {
    observer.OnAdvancedProtectionStatusChanged(is_under_advanced_protection);
  }
}

}  // namespace safe_browsing
