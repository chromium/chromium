// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTEXT_MENU_MODEL_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTEXT_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// The context menu model for birch bar. There are two types of bar menu:
// - Collapsed bar menu: the menu will be shown when right clicking in the
// Overview mode and there is no birch bar displayed. The menu has only one item
// to show the suggestions.
// - Expanded bar menu: the menu will be shown when right clicking in the
// Overview mode and there is a birch bar displayed. The menu includes
// customizing suggestion options.
class ASH_EXPORT BirchBarContextMenuModel : public ui::SimpleMenuModel {
 public:
  // The commands for the birch bar menu items.
  enum class ASH_EXPORT CommandId {
    kShowSuggestions = 0,   // Show/hide the birch bar with a switch button.
    kWeatherSuggestions,    // Show/hide the weather related suggestions.
    kCalendarSuggestions,   // Show/hide the Calendar related suggestions.
    kDriveSuggestions,      // Show/hide the Drive related suggestions.
    kChromeTabSuggestions,  // Show/hide Chrome tab suggestions.
    kMediaSuggestions,      // Show/hide media playing tab suggestions.
    kCoralSuggestions,      // Show/hide coral suggestions.
    kReset,                 // Reset preferences of all types of suggestions.
    kBarMenuEnd,
  };

  // The menu types detailed in the class description.
  enum class Type {
    kCollapsedBarMenu,
    kExpandedBarMenu,
  };

  BirchBarContextMenuModel(ui::SimpleMenuModel::Delegate* delegate, Type type);
  BirchBarContextMenuModel(const BirchBarContextMenuModel&) = delete;
  BirchBarContextMenuModel& operator=(const BirchBarContextMenuModel&) = delete;
  ~BirchBarContextMenuModel() override;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_CONTEXT_MENU_MODEL_H_
