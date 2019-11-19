// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_state_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#else
#include <stdint.h>
#include "content/public/browser/browser_accessibility_state.h"
#endif  // defined(OS_CHROMEOS)

namespace accessibility_state_utils {

bool IsScreenReaderEnabled() {
#if defined(OS_CHROMEOS)
  return chromeos::AccessibilityManager::Get() &&
         chromeos::AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
#else
  // TODO(katie): Can we use AXMode in Chrome OS as well? May need to stop
  // Switch Access and Select-to-Speak from setting kScreenReader.
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  return mode.has_mode(ui::AXMode::kScreenReader);
#endif  // defined(OS_CHROMEOS)
}

}  // namespace accessibility_state_utils
