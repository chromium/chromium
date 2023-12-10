// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_save_desk_button_container.h"

#include <array>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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

struct SaveDeskButtonStatus {
  bool enabled;
  int tooltip_id;
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

int GetTooltipID(SavedDeskSaveDeskButton::Type button_type,
                 TooltipStatus status) {
  switch (button_type) {
    case SavedDeskSaveDeskButton::Type::kSaveAsTemplate:
      return kSaveAsTemplateButtonTooltipIDs[static_cast<int>(status)];
    case SavedDeskSaveDeskButton::Type::kSaveForLater:
      return kSaveForLaterButtonTooltipIDs[static_cast<int>(status)];
  }
}

SaveDeskButtonStatus GetEnableStateAndTooltipIDForButtonType(
    SavedDeskSaveDeskButton::Type type,
    int current_entry_count,
    int max_entry_count,
    int incognito_window_count,
    int unsupported_window_count,
    int window_count) {
  // Disable if we already have the max supported saved desks.
  if (current_entry_count >= max_entry_count) {
    return {.enabled = false,
            .tooltip_id = GetTooltipID(type, TooltipStatus::kReachMax)};
  }

  // Enable if there are any supported window.
  if (incognito_window_count + unsupported_window_count != window_count) {
    return {.enabled = true,
            .tooltip_id = GetTooltipID(type, TooltipStatus::kOk)};
  }

  // Disable if there are incognito windows and unsupported Linux Apps but no
  // supported windows.
  if (incognito_window_count && unsupported_window_count) {
    return {.enabled = false,
            .tooltip_id = GetTooltipID(
                type, TooltipStatus::kIncognitoAndUnsupportedWindow)};
  }

  // Disable if there are incognito windows but no supported windows.
  if (incognito_window_count) {
    return {.enabled = false,
            .tooltip_id = GetTooltipID(type, TooltipStatus::kIncognitoWindow)};
  }

  // Disable if there are unsupported Linux Apps but no supported windows.
  DCHECK(unsupported_window_count);
  return {.enabled = false,
          .tooltip_id = GetTooltipID(type, TooltipStatus::kUnsupportedWindow)};
}

}  // namespace

class SavedDeskSaveDeskButtonContainer::
    SaveDeskButtonContainerAccessibilityObserver
    : public AccessibilityObserver {
 public:
  explicit SaveDeskButtonContainerAccessibilityObserver(
      const base::RepeatingClosure& accessibility_state_changed_callback)
      : accessibility_state_changed_callback_(
            accessibility_state_changed_callback) {
    observation_.Observe(Shell::Get()->accessibility_controller());
  }

  SaveDeskButtonContainerAccessibilityObserver(
      const SaveDeskButtonContainerAccessibilityObserver& other) = delete;
  SaveDeskButtonContainerAccessibilityObserver& operator=(
      const SaveDeskButtonContainerAccessibilityObserver& other) = delete;

  ~SaveDeskButtonContainerAccessibilityObserver() override = default;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override {
    accessibility_state_changed_callback_.Run();
  }
  void OnAccessibilityControllerShutdown() override { observation_.Reset(); }

 private:
  base::RepeatingClosure accessibility_state_changed_callback_;

  base::ScopedObservation<AccessibilityControllerImpl, AccessibilityObserver>
      observation_{this};
};

SavedDeskSaveDeskButtonContainer::SavedDeskSaveDeskButtonContainer(
    base::RepeatingClosure save_as_template_callback,
    base::RepeatingClosure save_for_later_callback) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetBetweenChildSpacing(kButtonSpacing);

  if (saved_desk_util::AreDesksTemplatesEnabled()) {
    save_desk_as_template_button_ =
        AddChildView(std::make_unique<SavedDeskSaveDeskButton>(
            save_as_template_callback,
            l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_AS_TEMPLATE_BUTTON),
            SavedDeskSaveDeskButton::Type::kSaveAsTemplate,
            &kSaveDeskAsTemplateIcon));
  }

  if (saved_desk_util::IsSavedDesksEnabled()) {
    save_desk_for_later_button_ =
        AddChildView(std::make_unique<SavedDeskSaveDeskButton>(
            save_for_later_callback,
            l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_FOR_LATER_BUTTON),
            SavedDeskSaveDeskButton::Type::kSaveForLater,
            &kSaveDeskForLaterIcon));
  }

  accessibility_observer_ =
      std::make_unique<SaveDeskButtonContainerAccessibilityObserver>(
          base::BindRepeating(&SavedDeskSaveDeskButtonContainer::
                                  UpdateButtonContainerForAccessibilityState,
                              base::Unretained(this)));
}

SavedDeskSaveDeskButtonContainer::~SavedDeskSaveDeskButtonContainer() = default;

void SavedDeskSaveDeskButtonContainer::UpdateButtonEnableStateAndTooltip(
    SavedDeskSaveDeskButton::Type type,
    int current_entry_count,
    int max_entry_count,
    int incognito_window_count,
    int unsupported_window_count,
    int window_count) {
  SavedDeskSaveDeskButton* button = GetButtonFromType(type);
  if (!button)
    return;
  SaveDeskButtonStatus button_status = GetEnableStateAndTooltipIDForButtonType(
      type, current_entry_count, max_entry_count, incognito_window_count,
      unsupported_window_count, window_count);
  button->SetEnabled(button_status.enabled);
  button->SetTooltipText(l10n_util::GetStringUTF16(button_status.tooltip_id));
}

void SavedDeskSaveDeskButtonContainer::
    UpdateButtonContainerForAccessibilityState() {
  // If Chromevox is turned on or off during the life span of this widget,
  // adjust to activatable or non-activatable accordingly.
  GetWidget()->widget_delegate()->SetCanActivate(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
}

SavedDeskSaveDeskButton* SavedDeskSaveDeskButtonContainer::GetButtonFromType(
    SavedDeskSaveDeskButton::Type type) {
  switch (type) {
    case SavedDeskSaveDeskButton::Type::kSaveAsTemplate:
      return save_desk_as_template_button_;
    case SavedDeskSaveDeskButton::Type::kSaveForLater:
      return save_desk_for_later_button_;
  }
}

BEGIN_METADATA(SavedDeskSaveDeskButtonContainer, views::BoxLayoutView)
END_METADATA

}  // namespace ash
