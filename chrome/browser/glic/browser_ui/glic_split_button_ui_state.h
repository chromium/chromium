// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_UI_STATE_H_
#define CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_UI_STATE_H_

#include <string>

namespace glic {

// Represents the UI state of the secondary slot of the Glic split button.
struct GlicSplitButtonUiState {
  enum class SecondaryAction {
    kNone,  // Unitary button mode (no secondary button shown)
    kText,  // Pill button: label + icon (e.g., "Check task", "Task complete")
    kChevron,  // Chevron icon
  };

  SecondaryAction secondary_action = SecondaryAction::kNone;
  std::u16string secondary_label;

  bool operator==(const GlicSplitButtonUiState& other) const = default;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_BROWSER_UI_GLIC_SPLIT_BUTTON_UI_STATE_H_
