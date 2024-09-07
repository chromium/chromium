// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_UTIL_H_
#define ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_UTIL_H_

#include <string>

#include "ash/wm/overview/birch/birch_bar_constants.h"
#include "ui/views/controls/button/button.h"

namespace ash::birch_bar_util {

// Creates a button for the glanceables chip with given `callback` and `label`,
// e.g. the join button of calendar chip.
std::unique_ptr<views::Button> CreateAddonButton(
    views::Button::PressedCallback callback,
    const std::u16string& label);

// Creates a weather temperature view which consists of two labels, one is for
// the temperature degree and the other is for the degree unit (Fahrenheit v.s.
// Celsius).
std::unique_ptr<views::View> CreateWeatherTemperatureView(
    const std::u16string& temp_str,
    bool fahrenheit);

// Gets suggestion type from the given command Id.
BirchSuggestionType CommandIdToSuggestionType(int command_id);

}  // namespace ash::birch_bar_util

#endif  // ASH_WM_OVERVIEW_BIRCH_BIRCH_BAR_UTIL_H_
