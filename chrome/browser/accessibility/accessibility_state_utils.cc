// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_state_utils.h"

#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#else
#include <stdint.h>
#include "content/public/browser/browser_accessibility_state.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace accessibility_state_utils {

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ::ash::AccessibilityManager;
#endif

bool IsScreenReaderEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return AccessibilityManager::Get() &&
         AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
#else
  // TODO(katie): Can we use AXMode in Chrome OS as well? May need to stop
  // Switch Access and Select-to-Speak from setting kScreenReader.
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  return mode.has_mode(ui::AXMode::kScreenReader);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace accessibility_state_utils
