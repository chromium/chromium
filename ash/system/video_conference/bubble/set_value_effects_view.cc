// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/set_value_effects_view.h"

#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"

namespace ash::video_conference {

namespace {

// A view with a label (for the effect name) and a tab slider that allows the
// user to select from one of several integer values.
class ValueButtonContainer : public views::View {
 public:
  explicit ValueButtonContainer(const VcHostedEffect* effect) {
    SetID(BubbleViewID::kSingleSetValueEffectView);
    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kVertical,
        /*inside_border_insets=*/gfx::Insets::TLBR(8, 0, 0, 0),
        /*between_child_spacing=*/8));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kStretch);

    if (!effect->label_text().empty()) {
      auto* label_container =
          AddChildView(std::make_unique<views::BoxLayoutView>());
      label_container->SetOrientation(
          views::BoxLayout::Orientation::kHorizontal);
      label_container->SetMainAxisAlignment(
          views::BoxLayout::MainAxisAlignment::kStart);
      label_container->SetInsideBorderInsets(gfx::Insets::TLBR(0, 8, 0, 0));

      label_container->AddChildView(
          std::make_unique<views::Label>(effect->label_text()));
      auto* spacer_view =
          label_container->AddChildView(std::make_unique<views::View>());
      // Let the spacer fill the remaining space, pushing the label to the
      // start.
      label_container->SetFlexForView(spacer_view, 1);
    }

    // If a container ID has been provided then assign it, otherwise assign the
    // default ID.
    SetID(effect->container_id().value_or(
        BubbleViewID::kSingleSetValueEffectView));

    // `effect` is expected to provide the current state of the effect, and
    // a `current_state` with no value means it couldn't be obtained.
    absl::optional<int> current_state = effect->get_state_callback().Run();
    DCHECK(current_state.has_value());

    auto tab_slider = std::make_unique<TabSlider>();
    const int num_states = effect->GetNumStates();
    DCHECK_LE(num_states, 3) << "UX Requests no more than 3 states, otherwise "
                                "the bubble will need to be wider.";
    for (int i = 0; i < num_states; ++i) {
      const VcEffectState* state = effect->GetState(/*index=*/i);
      auto* slider_button =
          tab_slider->AddButton(std::make_unique<IconLabelSliderButton>(
              state->button_callback(), state->icon(), state->label_text()));

      DCHECK(state->state().has_value());
      slider_button->SetSelected(state->state().value() == current_state);

      // See comments above `kSetValueButton*` in `BubbleViewID` for details
      // on how the IDs of these buttons are set.
      slider_button->SetID(i <= BubbleViewID::kSetValueButtonMax -
                                       BubbleViewID::kSetValueButtonMin
                               ? BubbleViewID::kSetValueButtonMin + i
                               : BubbleViewID::kSetValueButtonMax);
    }
    AddChildView(std::move(tab_slider));
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
      // If the current state of `effect` has no value, it means the state of
      // the effect cannot be obtained. This can happen if the
      // `VcEffectsDelegate` hosting `effect` has encountered an error or is
      // in some bad state. In this case its controls are not presented.
      if (!effect->get_state_callback().Run().has_value()) {
        continue;
      }

      AddChildView(std::make_unique<ValueButtonContainer>(effect));
    }
  }
}

}  // namespace ash::video_conference
