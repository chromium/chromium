// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/style_viewer/system_ui_components_grid_view_factories.h"

#include <string>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_viewer/system_ui_components_grid_view.h"

namespace ash {

namespace {

// Conigure of grid view for `PillButton` instances. We have 12 x 3 instances
// divided into 2 x 3 groups.
constexpr size_t kGridViewRowNum = 12;
constexpr size_t kGridViewColNum = 3;
constexpr size_t kGridViewRowGroupSize = 6;
constexpr size_t kGirdViewColGroupSize = 1;

// Converts the type flag of pill button's color variant to string.
std::u16string PillButtonColorTypeFlagToString(PillButton::TypeFlag type_flag) {
  switch (type_flag) {
    case PillButton::kDefault:
      return u"Default";
    case PillButton::kDefaultElevated:
      return u"DefaultElevated";
    case PillButton::kPrimary:
      return u"Primary";
    case PillButton::kSecondary:
      return u"Secondary";
    case PillButton::kFloating:
      return u"Floating";
    case PillButton::kAlert:
      return u"Alert";
    default:
      NOTREACHED() << "unknown color type" << type_flag;
  }
  return u"";
}

std::u16string PillButtonSizeTypeFlagToString(
    PillButton::TypeFlag size_type_flag) {
  if (size_type_flag == PillButton::kLarge)
    return u"Large";
  return u"";
}

std::u16string PillButtonIconTypeFlagToString(
    PillButton::TypeFlag icon_type_flag) {
  if (icon_type_flag == PillButton::kIconLeading)
    return u"With Leading Icon";

  if (icon_type_flag == PillButton::kIconFollowing)
    return u"With Following Icon";

  return u"";
}

}  // namespace

std::unique_ptr<SystemUIComponentsGridView>
CreatePillButtonInstancesGirdView() {
  auto grid_view = std::make_unique<SystemUIComponentsGridView>(
      kGridViewRowNum, kGridViewColNum, kGridViewRowGroupSize,
      kGirdViewColGroupSize);

  // Separate the color variants into two groups.
  const std::list<std::list<PillButton::TypeFlag>> color_variant_flag_groups{
      {PillButton::kDefault, PillButton::kDefaultElevated,
       PillButton::kPrimary},
      {PillButton::kSecondary, PillButton::kFloating, PillButton::kAlert}};

  // `PillButton` type features.
  const std::list<PillButton::TypeFlag> button_size_flags{0,
                                                          PillButton::kLarge};
  const std::list<PillButton::TypeFlag> icon_type_flags{
      0, PillButton::kIconLeading, PillButton::kIconFollowing};

  // Create an instance for each pill button type. The combination of the type
  // features is Type = Color Variant x Button Size x Icon type.
  for (auto color_variant_flags : color_variant_flag_groups) {
    for (auto icon_type_flag : icon_type_flags) {
      for (auto button_size_flag : button_size_flags) {
        for (auto color_type_flag : color_variant_flags) {
          // Convert flag type flags to name.
          const std::u16string name =
              PillButtonColorTypeFlagToString(color_type_flag) + u" " +
              PillButtonSizeTypeFlagToString(button_size_flag) + u" " +
              PillButtonIconTypeFlagToString(icon_type_flag);

          // Combine the type flags to get buttion type.
          PillButton::TypeFlag type =
              color_type_flag | button_size_flag | icon_type_flag;
          grid_view->AddInstance(
              name, std::make_unique<PillButton>(
                        PillButton::PressedCallback(), u"Pill Button",
                        static_cast<PillButton::Type>(type),
                        (icon_type_flag ? &kSettingsIcon : nullptr)));
        }
      }
    }
  }

  return grid_view;
}

}  // namespace ash
