// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_save_desk_button_container.h"

#include <array>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/public/cpp/desk_template.h"
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

  base::ScopedObservation<AccessibilityController, AccessibilityObserver>
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
            DeskTemplateType::kTemplate, &kSaveDeskAsTemplateIcon));
  }

  if (saved_desk_util::ShouldShowSavedDesksOptions()) {
    save_desk_for_later_button_ =
        AddChildView(std::make_unique<SavedDeskSaveDeskButton>(
            save_for_later_callback,
            l10n_util::GetStringUTF16(
                IDS_ASH_DESKS_TEMPLATES_SAVE_DESK_FOR_LATER_BUTTON),
            DeskTemplateType::kSaveAndRecall, &kSaveDeskForLaterIcon));
  }

  accessibility_observer_ =
      std::make_unique<SaveDeskButtonContainerAccessibilityObserver>(
          base::BindRepeating(&SavedDeskSaveDeskButtonContainer::
                                  UpdateButtonContainerForAccessibilityState,
                              base::Unretained(this)));
}

SavedDeskSaveDeskButtonContainer::~SavedDeskSaveDeskButtonContainer() = default;

void SavedDeskSaveDeskButtonContainer::UpdateButtonEnableStateAndTooltip(
    DeskTemplateType type,
    SaveDeskOptionStatus status) {
  SavedDeskSaveDeskButton* button = GetButtonFromType(type);
  if (!button) {
    return;
  }

  button->SetEnabled(status.enabled);
  button->SetTooltipText(l10n_util::GetStringUTF16(status.tooltip_id));
}

void SavedDeskSaveDeskButtonContainer::
    UpdateButtonContainerForAccessibilityState() {
  // If Chromevox is turned on or off during the life span of this widget,
  // adjust to activatable or non-activatable accordingly.
  GetWidget()->widget_delegate()->SetCanActivate(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
}

SavedDeskSaveDeskButton* SavedDeskSaveDeskButtonContainer::GetButtonFromType(
    DeskTemplateType type) {
  switch (type) {
    case DeskTemplateType::kTemplate:
      return save_desk_as_template_button_;
    case DeskTemplateType::kSaveAndRecall:
      return save_desk_for_later_button_;
    case DeskTemplateType::kFloatingWorkspace:
    case DeskTemplateType::kUnknown:
      return nullptr;
  }
}

BEGIN_METADATA(SavedDeskSaveDeskButtonContainer)
END_METADATA

}  // namespace ash
