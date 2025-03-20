// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/advanced_protection_status_manager_android.h"

#include "base/command_line.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"

namespace safe_browsing {

AdvancedProtectionStatusManagerAndroid::
    AdvancedProtectionStatusManagerAndroid() = default;
AdvancedProtectionStatusManagerAndroid::
    ~AdvancedProtectionStatusManagerAndroid() = default;

bool AdvancedProtectionStatusManagerAndroid::IsUnderAdvancedProtection() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceTreatUserAsAdvancedProtection)) {
    return true;
  }

  return is_under_advanced_protection_;
}

void AdvancedProtectionStatusManagerAndroid::AddObserver(
    StatusChangedObserver* observer) {}

void AdvancedProtectionStatusManagerAndroid::RemoveObserver(
    StatusChangedObserver* observer) {}

void AdvancedProtectionStatusManagerAndroid::
    SetAdvancedProtectionStatusForTesting(bool enrolled) {
  is_under_advanced_protection_ = enrolled;
}

}  // namespace safe_browsing
