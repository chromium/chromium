// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_TAB_INDICATOR_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_TAB_INDICATOR_STATE_H_

namespace actor::ui {
// Enum for status of the tab indicator (icon to the right of the tab title).
enum class TabIndicatorStatus {
  // Not visible.
  kNone,
  // Icon visible. Spinner not visible.
  kStatic,
  // Spinner visible. Icon not visible.
  kDynamic,
};
}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_TAB_INDICATOR_STATE_H_
