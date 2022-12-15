// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/set_value_effects_view.h"

#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/flex_layout.h"

namespace ash::video_conference {

namespace {

// A view with label (for the effect name) that allows the user to select from
// one of several integer values. TODO(b/253273036) Implement this as a
// tab-slider view instead of a radio switch.
class ValueButtonContainer : public views::View {
 public:
  explicit ValueButtonContainer(const VcHostedEffect* effect) {
    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

    if (!effect->label_text().empty()) {
      AddChildView(std::make_unique<views::Label>(effect->label_text()));
    }

    // Add a button for each state.
    for (int i = 0; i < effect->GetNumStates(); ++i) {
      const VcEffectState* state = effect->GetState(/*index=*/i);
      std::unique_ptr<views::RadioButton> state_button =
          std::make_unique<views::RadioButton>(state->label_text(),
                                               /*group_id=*/effect->id());
      state_button->SetCallback(state->button_callback());

      // See comments above `kSetValueButton*` in `BubbleViewID` for details
      // on how the IDs of these buttons are set.
      state_button->SetID(i <= BubbleViewID::kSetValueButtonMax -
                                      BubbleViewID::kSetValueButtonMin
                              ? BubbleViewID::kSetValueButtonMin + i
                              : BubbleViewID::kSetValueButtonMax);

      AddChildView(std::move(state_button));
    }

    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(10, 10)));
    SetBackground(views::CreateRoundedRectBackground(gfx::kGoogleGreen800,
                                                     /*radius=*/10));
  }

  ValueButtonContainer(const ValueButtonContainer&) = delete;
  ValueButtonContainer& operator=(const ValueButtonContainer&) = delete;

  ~ValueButtonContainer() override = default;
};

}  // namespace

SetValueEffectsView::SetValueEffectsView(
    VideoConferenceTrayController* controller) {
  SetID(BubbleViewID::kSetValueEffectsView);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  if (controller->effects_manager().HasSetValueEffects()) {
    for (auto* effect : controller->effects_manager().GetSetValueEffects()) {
      AddChildView(std::make_unique<ValueButtonContainer>(effect));
    }
  }
}

}  // namespace ash::video_conference