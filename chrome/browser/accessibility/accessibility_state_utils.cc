// Copyright 2019 The Chromium Authors
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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/embedded_a11y_manager_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace accessibility_state_utils {

enum class OverrideStatus { kNotSet = 0, kEnabled = 1, kDisabled = 2 };

static OverrideStatus screen_reader_enabled_override_for_testing =
    OverrideStatus::kNotSet;

#if BUILDFLAG(IS_CHROMEOS_ASH)
using ::ash::AccessibilityManager;
#endif

bool IsScreenReaderEnabled() {
  if (screen_reader_enabled_override_for_testing != OverrideStatus::kNotSet) {
    return screen_reader_enabled_override_for_testing ==
           OverrideStatus::kEnabled;
  }
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

void OverrideIsScreenReaderEnabledForTesting(bool enabled) {
  screen_reader_enabled_override_for_testing =
      enabled ? OverrideStatus::kEnabled : OverrideStatus::kDisabled;
}

bool IsSelectToSpeakEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return AccessibilityManager::Get() &&
         AccessibilityManager::Get()->IsSelectToSpeakEnabled();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return EmbeddedA11yManagerLacros::GetInstance() &&
         EmbeddedA11yManagerLacros::GetInstance()->IsSelectToSpeakEnabled();
#else
  return false;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace accessibility_state_utils
