// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_IDS_H_
#define ASH_SYSTEM_PALETTE_PALETTE_IDS_H_

namespace ash {

// Palette tools are grouped into different categories. Each tool corresponds to
// exactly one group, and at most one tool can be active per group. Actions are
// actions the user wants to do, such as take a screenshot, and modes generally
// change OS behavior, like showing a laser pointer instead of a cursor. A mode
// is active until the user completes the action or disables it.
enum class PaletteGroup { ACTION, MODE };

enum class PaletteToolId {
  NONE,
  CREATE_NOTE,
  LASER_POINTER,
  MAGNIFY,
  ENTER_CAPTURE_MODE,
  MARKER_MODE,
};

// Usage of each pen palette option. This enum is used to back an UMA histogram
// and should be treated as append-only.
enum PaletteTrayOptions {
  PALETTE_CLOSED_NO_ACTION = 0,
  PALETTE_SETTINGS_BUTTON,
  PALETTE_HELP_BUTTON,
  PALETTE_CAPTURE_REGION,
  PALETTE_CAPTURE_SCREEN,
  PALETTE_NEW_NOTE,
  PALETTE_MAGNIFY,
  PALETTE_LASER_POINTER,
  PALETTE_METALAYER,
  PALETTE_ENTER_CAPTURE_MODE,
  PALETTE_MARKER_MODE,
  PALETTE_OPTIONS_COUNT
};

// Type of palette mode cancellation. This enum is used to back an UMA histogram
// and should be treated as append-only.
enum PaletteModeCancelType {
  PALETTE_MODE_LASER_POINTER_CANCELLED = 0,
  PALETTE_MODE_LASER_POINTER_SWITCHED,
  PALETTE_MODE_MAGNIFY_CANCELLED,
  PALETTE_MODE_MAGNIFY_SWITCHED,
  PALETTE_MODE_CANCEL_TYPE_COUNT
};

// Type of palette option invocation method.
enum class PaletteInvocationMethod {
  MENU,
  SHORTCUT,
};

// Helper functions that convert PaletteToolIds to PaletteTrayOptions.
PaletteTrayOptions PaletteToolIdToPaletteTrayOptions(PaletteToolId tool_id);

// Helper functions that convert PaletteToolIds to PaletteModeCancelType.
PaletteModeCancelType PaletteToolIdToPaletteModeCancelType(
    PaletteToolId tool_id,
    bool is_switched);

}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_IDS_H_
