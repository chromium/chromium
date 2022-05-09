// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/save_desk_template_button_container.h"

#include <array>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/vector_icon_types.h"

namespace ash {

namespace {

constexpr int kButtonSpacing = 16;

enum class TooltipStatus {
  kOk = 0,
  kReachMax,
  kIncognitoWindow,
  kUnsupportedWindow,
  kIncognitoAndUnsupportedWindow,
  kNumberOfTooltipStatus,
};

constexpr std::array<int,
                     static_cast<int>(TooltipStatus::kNumberOfTooltipStatus)>
    kSaveAsTemplateButtonTooltipIDs = {
        IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_AS_TEMPLATE_BUTTON,
        IDS_ASH_DESKS_TEMPLATES_MAX_TEMPLATES_TOOLTIP,
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_INCOGNITO_TOOLTIP,
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_LINUX_APPS_TOOLTIP,
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_LINUX_APPS_AND_INCOGNITO_TOOLTIP,
};

constexpr std::array<int,
                     static_cast<int>(TooltipStatus::kNumberOfTooltipStatus)>
    kSaveForLaterButtonTooltipIDs = {
        IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_FOR_LATER_BUTTON,
        IDS_ASH_DESKS_TEMPLATES_MAX_SAVED_DESKS_TOOLTIP,
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_INCOGNITO_TOOLTIP,
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_LINUX_APPS_TOOLTIP,
        IDS_ASH_DESKS_TEMPLATES_UNSUPPORTED_LINUX_APPS_AND_INCOGNITO_TOOLTIP,
};

int GetTooltipID(SaveDeskTemplateButton::Type button_type,
                 TooltipStatus status) {
  switch (button_type) {
    case SaveDeskTemplateButton::Type::kSaveAsTemplate:
      return kSaveAsTemplateButtonTooltipIDs[static_cast<int>(status)];
    case SaveDeskTemplateButton::Type::kSaveForLater:
      return kSaveForLaterButtonTooltipIDs[static_cast<int>(status)];
  }
}

std::pair<bool, int> GetEnableStateAndTooltipIDForButtonType(
    SaveDeskTemplateButton::Type type,
    int current_entry_count,
    int max_entry_count,
    int incognito_window_count,
    int unsupported_window_count,
    int window_count) {
  // Disable if we already have the max supported saved desks.
  if (current_entry_count >= max_entry_count) {
    return {/*enabled=*/false,
            /*tooltip_ID=*/GetTooltipID(type, TooltipStatus::kReachMax)};
  }

  // Enable if there are any supported window.
  if (incognito_window_count + unsupported_window_count != window_count) {
    return {/*enabled=*/true,
            /*tooltip_ID=*/GetTooltipID(type, TooltipStatus::kOk)};
  }

  // Disable if there are incognito windows and unsupported Linux Apps but no
  // supported windows.
  if (incognito_window_count && unsupported_window_count) {
    return {/*enabled=*/false,
            /*tooltip_ID=*/GetTooltipID(
                type, TooltipStatus::kIncognitoAndUnsupportedWindow)};
  }

  // Disable if there are incognito windows but no supported windows.
  if (incognito_window_count) {
    return {/*enabled=*/false,
            /*tooltip_ID=*/GetTooltipID(type, TooltipStatus::kIncognitoWindow)};
  }

  // Disable if there are unsupported Linux Apps but no supported windows.
  DCHECK(unsupported_window_count);
  return {/*enabled=*/false,
          /*tooltip_ID=*/GetTooltipID(type, TooltipStatus::kUnsupportedWindow)};
}

}  // namespace

SaveDeskTemplateButtonContainer::SaveDeskTemplateButtonContainer(
    base::RepeatingClosure save_as_template_callback,
    base::RepeatingClosure save_for_later_callback) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBetweenChildSpacing(kButtonSpacing);

  if (saved_desk_util::AreDesksTemplatesEnabled()) {
    save_desk_as_template_button_ =
        AddChildView(std::make_unique<SaveDeskTemplateButton>(
            save_as_template_callback,
            l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_AS_TEMPLATE_BUTTON),
            SaveDeskTemplateButton::Type::kSaveAsTemplate,
            &kSaveDeskAsTemplateIcon));
  }

  if (saved_desk_util::IsDeskSaveAndRecallEnabled()) {
    save_desk_for_later_button_ =
        AddChildView(std::make_unique<SaveDeskTemplateButton>(
            save_for_later_callback,
            l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_FOR_LATER_BUTTON),
            SaveDeskTemplateButton::Type::kSaveForLater,
            &kSaveDeskForLaterIcon));
  }
}

void SaveDeskTemplateButtonContainer::UpdateButtonEnableStateAndTooltip(
    SaveDeskTemplateButton::Type type,
    int current_entry_count,
    int max_entry_count,
    int incognito_window_count,
    int unsupported_window_count,
    int window_count) {
  SaveDeskTemplateButton* button = GetButtonFromType(type);
  if (!button)
    return;
  std::pair<bool, int> enable_state_and_tooltip_ID =
      GetEnableStateAndTooltipIDForButtonType(
          type, current_entry_count, max_entry_count, incognito_window_count,
          unsupported_window_count, window_count);
  button->SetEnabled(enable_state_and_tooltip_ID.first);
  button->SetTooltipText(
      l10n_util::GetStringUTF16(enable_state_and_tooltip_ID.second));
}

SaveDeskTemplateButton* SaveDeskTemplateButtonContainer::GetButtonFromType(
    SaveDeskTemplateButton::Type type) {
  switch (type) {
    case SaveDeskTemplateButton::Type::kSaveAsTemplate:
      return save_desk_as_template_button_;
    case SaveDeskTemplateButton::Type::kSaveForLater:
      return save_desk_for_later_button_;
  }
}

}  // namespace ash
