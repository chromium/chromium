// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_TABLE_H_

#include <stddef.h>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

// The complete list of Ash accelerators is in ash/public/cpp/accelerators.h.
// This file mainly keeps track of special categories of accelerator.
//
// There are five classes of accelerators in Ash:
//
// Ash (OS) reserved:
// * Neither packaged apps nor web pages can cancel.
// * For example, power button.
// * See kReservedActions below.
//
// Ash (OS) preferred:
// * Fullscreen window can consume, but normal window can't.
// * For example, Alt-Tab window cycling.
// * See kPreferredActions below.
//
// Chrome OS system keys:
// * For legacy reasons, v1 apps can process and cancel. Otherwise handled
//   directly by Ash.
// * Brightness, volume control, etc.
// * See IsSystemKey() in ash/accelerators/accelerator_filter.cc.
//
// Browser reserved:
// * Packaged apps can cancel but web pages cannot.
// * For example, browser back and forward from first-row function keys.
// * See IsReservedCommandOrKey() in
//   chrome/browser/ui/browser_command_controller.cc.
//
// Browser non-reserved:
// * Both packaged apps and web pages can cancel.
// * For example, selecting tabs by number with Ctrl-1 to Ctrl-9.
// * See kAcceleratorMap in chrome/browser/ui/views/accelerator_table.cc.
//
// In particular, there is not an accelerator processing pass for Ash after
// the browser gets the accelerator.  See crbug.com/285308 for details.
//
// There are also various restrictions on accelerators allowed at the login
// screen, when running in "forced app mode" (like a kiosk), etc. See the
// various kActionsAllowed* below.

// Gathers the needed data to handle deprecated accelerators.
struct DeprecatedAcceleratorData {
  // The action that has deprecated accelerators.
  AcceleratorAction action;

  // The name of the UMA histogram that will be used to measure the deprecated
  // v.s. new accelerator usage.
  const char* uma_histogram_name;

  // The ID of the localized notification message to show to users informing
  // them about the deprecation.
  int notification_message_id;

  // The ID of the localized new shortcut key.
  int new_shortcut_id;

  // The replacement of the deprecated accelerator.
  ui::Accelerator replacement;

  // Specifies whether the deprecated accelerator is still enabled to do its
  // associated action.
  bool deprecated_enabled;

  // The accelerator name in the pref dict to check if a deprecated accelerator
  // notification is displayed 3 times or within the last 24 hours.
  const char* pref_name;
};

// This will be used for the UMA stats to measure the how many users are using
// the old v.s. new accelerators.
enum DeprecatedAcceleratorUsage {
  DEPRECATED_USED = 0,     // The deprecated accelerator is used.
  NEW_USED,                // The new accelerator is used.
  DEPRECATED_USAGE_COUNT,  // Maximum value of this enum for histogram use.
};

// The list of the deprecated accelerators.
ASH_EXPORT extern const AcceleratorData kDeprecatedAccelerators[];
ASH_EXPORT extern const size_t kDeprecatedAcceleratorsLength;

// The list of the actions with deprecated accelerators and the needed data to
// handle them.
ASH_EXPORT extern const DeprecatedAcceleratorData kDeprecatedAcceleratorsData[];
ASH_EXPORT extern const size_t kDeprecatedAcceleratorsDataLength;

// Debug accelerators. Debug accelerators are only enabled when the "Debugging
// keyboard shortcuts" flag (--ash-debug-shortcuts) is enabled. Debug actions
// are always run (similar to reserved actions). Debug accelerators can be
// enabled in about:flags.
ASH_EXPORT extern const AcceleratorData kDebugAcceleratorData[];
ASH_EXPORT extern const size_t kDebugAcceleratorDataLength;

// Developer accelerators that are enabled only with the command-line switch
// --ash-dev-shortcuts. They are always run similar to reserved actions.
ASH_EXPORT extern const AcceleratorData kDeveloperAcceleratorData[];
ASH_EXPORT extern const size_t kDeveloperAcceleratorDataLength;

// Actions that should be handled very early in Ash unless the current target
// window is full-screen.
ASH_EXPORT extern const AcceleratorAction kPreferredActions[];
ASH_EXPORT extern const size_t kPreferredActionsLength;

// Actions that are always handled in Ash.
ASH_EXPORT extern const AcceleratorAction kReservedActions[];
ASH_EXPORT extern const size_t kReservedActionsLength;

// Actions allowed while user is not signed in or screen is locked.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLoginOrLockScreen[];
ASH_EXPORT extern const size_t kActionsAllowedAtLoginOrLockScreenLength;

// Actions allowed while screen is locked (in addition to
// kActionsAllowedAtLoginOrLockScreen).
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtLockScreen[];
ASH_EXPORT extern const size_t kActionsAllowedAtLockScreenLength;

// Actions allowed while power menu is opened.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtPowerMenu[];
ASH_EXPORT extern const size_t kActionsAllowedAtPowerMenuLength;

// Actions allowed while a modal window is up.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedAtModalWindow[];
ASH_EXPORT extern const size_t kActionsAllowedAtModalWindowLength;

// Actions which may be repeated by holding an accelerator key.
ASH_EXPORT extern const AcceleratorAction kRepeatableActions[];
ASH_EXPORT extern const size_t kRepeatableActionsLength;

// Actions allowed in app mode or pinned mode.
ASH_EXPORT extern const AcceleratorAction
    kActionsAllowedInAppModeOrPinnedMode[];
ASH_EXPORT extern const size_t kActionsAllowedInAppModeOrPinnedModeLength;

// Actions that can be performed in pinned mode.
// In pinned mode, the action listed in this or "in app mode or pinned mode"
// table can be performed.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedInPinnedMode[];
ASH_EXPORT extern const size_t kActionsAllowedInPinnedModeLength;

// Actions that can be performed in app mode.
// In app mode, the action listed in this or "in app mode or pinned mode" table
// can be performed.
ASH_EXPORT extern const AcceleratorAction kActionsAllowedInAppMode[];
ASH_EXPORT extern const size_t kActionsAllowedInAppModeLength;

// Actions that require at least 1 window.
ASH_EXPORT extern const AcceleratorAction kActionsNeedingWindow[];
ASH_EXPORT extern const size_t kActionsNeedingWindowLength;

// Actions that can be performed while keeping the menu open.
ASH_EXPORT extern const AcceleratorAction kActionsKeepingMenuOpen[];
ASH_EXPORT extern const size_t kActionsKeepingMenuOpenLength;

// Actions that are duplicated with browser shortcuts.
ASH_EXPORT extern const AcceleratorAction kActionsDuplicatedWithBrowser[];
ASH_EXPORT extern const size_t kActionsDuplicatedWithBrowserLength;

// Actions that are interceptable by browser.
// These actions are ash's shortcuts, but they are sent to the browser
// once in order to make it interceptable by webpage/apps.
ASH_EXPORT extern const AcceleratorAction kActionsInterceptableByBrowser[];
ASH_EXPORT extern const size_t kActionsInterceptableByBrowserLength;

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_TABLE_H_
