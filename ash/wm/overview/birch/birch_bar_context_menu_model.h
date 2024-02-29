// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTEXT_MENU_MODEL_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTEXT_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// The context menu model for birch bar and birch chips. The model is for three
// types of menus:
// - Collapsed bar menu: the menu will be shown when right clicking in the
// Overview mode and there is no birch bar displayed. The menu has only one item
// to show the suggestions.
// - Expanded bar menu: the menu will be shown when right clicking in the
// Overview mode and there is a birch bar displayed. The menu includes
// customizing suggestion options.
// - Chip menu: the menu will be shown when right clicking on a birch chip. The
// menu allows user to remove the chip, hide Drive suggestions, customize
// suggestions, and send feedback. The item of customizing suggestions will pop
// out the expanded bar menu.
class ASH_EXPORT BirchBarContextMenuModel : public ui::SimpleMenuModel {
 public:
  enum class CommandId {
    // The commands for the birch chip menu items.
    kHideSuggestion,        // Hide current chip.
    kHideDriveSuggestions,  // Hide all Drive related chips.
    kCustomizeSuggestions,  // Pop out the expanded bar menu with customizing
                            // suggestions options.
    kFeedback,              // Send user feedback for birch bar.

    // The commands for the birch bar menu items.
    kShowSuggestions,         // Show/hide the birch bar with a switch button.
    kWeatherSuggestions,      // Show/hide the weather related suggestions.
    kCalendarSuggestions,     // Show/hide the Calendar related suggestions.
    kDriveSuggestions,        // Show/hide the Drive related suggestions.
    kYouTubeSuggestions,      // Show/hide the YouTube related suggestions.
    kOtherDeviceSuggestions,  // Show/hide the suggestions from other device.
    kReset,                   // Reset preferences of all types of suggestions.
  };

  // The three menu types detailed in the class description.
  enum class Type {
    kChipMenu,
    kCollapsedBarMenu,
    kExpandedBarMenu,
  };

  BirchBarContextMenuModel(ui::SimpleMenuModel::Delegate* delegate, Type type);
  BirchBarContextMenuModel(const BirchBarContextMenuModel&) = delete;
  BirchBarContextMenuModel& operator=(const BirchBarContextMenuModel&) = delete;
  ~BirchBarContextMenuModel() override;

 private:
  // Adds items for bar menu.
  void AddBarMenuItems();

  // Adds items for chip menu.
  void AddChipMenuItems();

  const Type type_;

  // The model for chip menu's submenu which is a full bar menu.
  std::unique_ptr<BirchBarContextMenuModel> sub_menu_model_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTEXT_MENU_MODEL_H_
