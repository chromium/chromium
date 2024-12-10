// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_state_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#else
#include <stdint.h>
#include "content/public/browser/browser_accessibility_state.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace accessibility_state_utils {

enum class OverrideStatus { kNotSet = 0, kEnabled = 1, kDisabled = 2 };

static OverrideStatus screen_reader_enabled_override_for_testing =
    OverrideStatus::kNotSet;

#if BUILDFLAG(IS_CHROMEOS)
using ::ash::AccessibilityManager;
#endif

bool IsScreenReaderEnabled() {
  if (screen_reader_enabled_override_for_testing != OverrideStatus::kNotSet) {
    return screen_reader_enabled_override_for_testing ==
           OverrideStatus::kEnabled;
  }
#if BUILDFLAG(IS_CHROMEOS)
  return AccessibilityManager::Get() &&
         AccessibilityManager::Get()->IsSpokenFeedbackEnabled();
#else
  // TODO(crbug.com/383057958): Consider updating this to return true only when
  // an actual screen reader is enabled.
  ui::AXMode mode =
      content::BrowserAccessibilityState::GetInstance()->GetAccessibilityMode();
  return mode.has_mode(ui::AXMode::kScreenReader);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void OverrideIsScreenReaderEnabledForTesting(bool enabled) {
  screen_reader_enabled_override_for_testing =
      enabled ? OverrideStatus::kEnabled : OverrideStatus::kDisabled;
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
