// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_state_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/platform/ax_platform.h"

namespace accessibility_state_utils {

#if BUILDFLAG(IS_CHROMEOS)
using ::ash::AccessibilityManager;
#endif  // BUILDFLAG(IS_CHROMEOS)

bool IsScreenReaderEnabled() {
  return ui::AXPlatform::GetInstance().IsScreenReaderActive();
}

void OverrideIsScreenReaderEnabledForTesting(bool enabled) {
  content::BrowserAccessibilityState::GetInstance()->SetScreenReaderAppActive(
      enabled);
}

bool IsSelectToSpeakEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return AccessibilityManager::Get() &&
         AccessibilityManager::Get()->IsSelectToSpeakEnabled();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace accessibility_state_utils
