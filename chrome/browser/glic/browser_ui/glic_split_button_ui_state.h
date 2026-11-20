// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_UI_STATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_UI_STATE_H_

#include <string>

namespace glic {

// Represents the UI state of the secondary slot of the Glic split button.
struct GlicSplitButtonUiState {
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.glic
  enum class SecondaryAction {
    // Unitary button mode (no secondary button shown).
    kNone = 0,
    // Pill button: label + icon (e.g., "Check task", "Task complete").
    kText = 1,
    // Chevron icon.
    kChevron = 2,
  };

  // Icon shown in the secondary pill when `secondary_action` is `kText`.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.glic
  enum class SecondaryIcon {
    // No alert-specific icon (used when `secondary_action` is `kNone` or
    // `kChevron`).
    kNone = 0,
    // Activity requires user attention (cursor spark).
    kCursorSpark = 1,
    // One or more automated tasks finished (completed task spark).
    kTaskSpark = 2,
    // Background conversation response is ready (chat spark).
    kChatSpark = 3,
  };

  SecondaryAction secondary_action = SecondaryAction::kNone;
  SecondaryIcon secondary_icon = SecondaryIcon::kNone;
  std::u16string secondary_label;

  bool operator==(const GlicSplitButtonUiState& other) const = default;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_UI_STATE_H_
