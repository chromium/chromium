// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_CONTEXT_MENU_MODEL_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_CONTEXT_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ash/wm/overview/birch/birch_bar_context_menu_model.h"
#include "base/types/cxx23_to_underlying.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// Menu for different suggestion types of chips: the menu will be shown when
// right clicking on a birch chip. The menu allows user to remove the chip, hide
// current type of suggestions, customize suggestions, and send feedback. The
// item of customizing suggestions will pop out the expanded bar menu.
class BirchChipContextMenuModel : public ui::SimpleMenuModel {
 public:
  // The commands for the birch chip menu items.
  enum class ASH_EXPORT CommandId {
    // To avoid conflicting with the command IDs of bar menu, start enum value
    // after the ending value of `BirchBarContextMenuModel`.
    kHideSuggestion =
        base::to_underlying(BirchBarContextMenuModel::CommandId::kBarMenuEnd) +
        1,                      // Hide current chip.
    kHideWeatherSuggestions,    // Hide all weather related chips.
    kToggleTemperatureUnits,    // Toggles between F and C.
    kHideCalendarSuggestions,   // Hide all calendar related chips.
    kHideDriveSuggestions,      // Hide all Drive related chips.
    kHideChromeTabSuggestions,  // Hide all Chrome tab related chips.
    kHideMediaSuggestions,      // Hide all media related chips.
    kHideCoralSuggestions,      // Hide all coral related chips.
    kCustomizeSuggestions,  // Pop out the expanded bar menu with customizing
                            // suggestions options.
    kCoralNewDesk,          // Open coral in a new desk.
    kCoralSaveForLater,     // Save coral for later.
    kProvideFeedback,       // Pop out UI for collecting feature feedback.
  };

  BirchChipContextMenuModel(ui::SimpleMenuModel::Delegate* delegate,
                            BirchSuggestionType chip_type);
  BirchChipContextMenuModel(const BirchChipContextMenuModel&) = delete;
  BirchChipContextMenuModel& operator=(const BirchChipContextMenuModel&) =
      delete;
  ~BirchChipContextMenuModel() override;

 private:
  std::unique_ptr<BirchBarContextMenuModel> sub_menu_model_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_CHIP_CONTEXT_MENU_MODEL_H_
