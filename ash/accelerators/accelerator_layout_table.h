// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_
#define ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_

#include <cstdint>
#include <functional>

#include "ash/ash_export.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "base/containers/fixed_flat_map.h"

namespace ash {

// Contains details for UI styling of an accelerator.
struct ASH_EXPORT AcceleratorLayoutDetails {
  // The accelerator action id associated for a source. Concat `source` and
  // `action_id` to get a unique identifier for an accelerator action.
  uint32_t action_id;

  // Category of the accelerator.
  mojom::AcceleratorCategory category;

  // Subcategory of the accelerator.
  mojom::AcceleratorSubcategory sub_category;

  // True if the accelerator cannot be modified through customization.
  // False if the accelerator can be modified through customization.
  bool locked;

  // The layout style of the accelerator, this provides additional context
  // on how to accelerator should be represented in the UI.
  mojom::AcceleratorLayoutStyle layout_style;

  // The source of which the accelerator is from.
  mojom::AcceleratorSource source;
};

// A fixed array of accelerator layouts used for categorization and styling of
// accelerator actions. The ordering of the array is important and is used
// 1:1 for displaying shortcuts in the shortcut customization app.
// Adding an accelerator layout in this array will create a new entry in the
// app.
// TODO(jimmyxgong): This is a stub map with stub details, replace with real
// one when categorization is available.
ASH_EXPORT constexpr AcceleratorLayoutDetails kAcceleratorLayouts[] = {
    // Tab & Windows.
    {DESKS_ACTIVATE_DESK_LEFT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_ACTIVATE_DESK_RIGHT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_NEW_DESK, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {WINDOW_CYCLE_SNAP_LEFT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {WINDOW_CYCLE_SNAP_RIGHT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_MAXIMIZED, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {WINDOW_MINIMIZE, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {MOVE_ACTIVE_WINDOW_BETWEEN_DISPLAYS,
     mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_MOVE_ACTIVE_ITEM_LEFT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_MOVE_ACTIVE_ITEM_RIGHT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_CROSH, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {ROTATE_WINDOW, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {CYCLE_FORWARD_MRU, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TAKE_WINDOW_SCREENSHOT, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_FULLSCREEN, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_RESIZE_LOCK_MENU, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {DESKS_REMOVE_CURRENT_DESK, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {UNPIN, mojom::AcceleratorCategory::kTabsAndWindows,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // Page and Web Browser.
    {FOCUS_PREVIOUS_PANE, mojom::AcceleratorCategory::kPageAndWebBrowser,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // System and display settings.
    {SHOW_TASK_MANAGER, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {LAUNCH_LAST_APP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SCALE_UI_DOWN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SCALE_UI_UP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_MIRROR_MODE, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {VOLUME_MUTE, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_DIAGNOSTICS, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_GET_HELP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_FILE_MANAGER, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_SYSTEM_TRAY_BUBBLE,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_APP_LIST, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SUSPEND, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SCALE_UI_RESET, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {ROTATE_SCREEN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_MESSAGE_CENTER_BUBBLE,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SHOW_STYLUS_TOOLS, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_IME_MENU_BUBBLE,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {EXIT, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {OPEN_FEEDBACK_PAGE, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWAP_PRIMARY_DISPLAY,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_LAST_USED_IME,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_NEXT_IME, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_NEXT_USER, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SWITCH_TO_PREVIOUS_USER,
     mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {BRIGHTNESS_DOWN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {BRIGHTNESS_UP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {VOLUME_DOWN, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {VOLUME_UP, mojom::AcceleratorCategory::kSystemAndDisplaySettings,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // Text Editing.
    {KEYBOARD_BRIGHTNESS_DOWN, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {KEYBOARD_BRIGHTNESS_UP, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_CLIPBOARD_HISTORY, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {SHOW_EMOJI_PICKER, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_CAPS_LOCK, mojom::AcceleratorCategory::kTextEditing,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},

    // Accessibility
    {FOCUS_SHELF, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_SPOKEN_FEEDBACK, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kGeneral,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_HIGH_CONTRAST, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_DOCKED_MAGNIFIER, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {TOGGLE_FULLSCREEN_MAGNIFIER, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemControls,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {MAGNIFIER_ZOOM_IN, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
    {MAGNIFIER_ZOOM_OUT, mojom::AcceleratorCategory::kAccessibility,
     mojom::AcceleratorSubcategory::kSystemApps,
     /*locked=*/true, mojom::AcceleratorLayoutStyle::kDefault,
     mojom::AcceleratorSource::kAsh},
};
}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_LAYOUT_TABLE_H_
