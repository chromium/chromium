
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/toggle_effects_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kButtonCornerRadius = 16;
constexpr int kButtonHeight = 64;

// A single toggle button for a video conference effect, combined with a text
// label.
class ButtonContainer : public views::View, public IconButton::Delegate {
 public:
  METADATA_HEADER(ButtonContainer);

  ButtonContainer(views::Button::PressedCallback callback,
                  const gfx::VectorIcon* icon,
                  bool toggle_state,
                  const std::u16string& label_text,
                  const int accessible_name_id,
                  const int preferred_width) {
    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

    // This makes the view the expand or contract to occupy any available space.
    SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kUnbounded));

    // `preferred_width` is assigned by the row this buttons resides in,
    // `kButtonHeight` is from the spec.
    SetPreferredSize(gfx::Size(preferred_width, kButtonHeight));

    // Construct the `IconButton`, set ID and initial toggle state (from the
    // passed-in value, which is the current state of the effect).
    std::unique_ptr<IconButton> button = std::make_unique<IconButton>(
        callback, IconButton::Type::kMedium, icon, accessible_name_id,
        /*is_togglable=*/true,
        /*has_border=*/true);
    button->SetID(video_conference::BubbleViewID::kToggleEffectsButton);
    button->SetToggled(toggle_state);

    // Delegate is the `ButtonContainer`, which changes the toggle-state of the
    // button when clicked.
    button->set_delegate(this);

    // `button` is owned by the `view::View` but a pointer is saved off for
    // logic based on its toggle state.
    button_ = AddChildView(std::move(button));

    // Label is below the button.
    AddChildView(std::make_unique<views::Label>(label_text));

    UpdateColorsAndBackground();
  }

  ButtonContainer(const ButtonContainer&) = delete;
  ButtonContainer& operator=(const ButtonContainer&) = delete;

  ~ButtonContainer() override = default;

  // IconButton::Delegate:
  void OnButtonToggled(IconButton* button) override {}
  void OnButtonClicked(IconButton* button) override {
    button->SetToggled(!button->toggled());
    UpdateColorsAndBackground();
  }

  void UpdateColorsAndBackground() {
    ui::ColorId background_color_id =
        button_->toggled() ? cros_tokens::kCrosSysSystemPrimaryContainer
                           : cros_tokens::kCrosSysSystemOnBase;

    SetBackground(views::CreateThemedRoundedRectBackground(
        background_color_id, kButtonCornerRadius));
  }

 private:
  IconButton* button_ = nullptr;
};

BEGIN_METADATA(ButtonContainer, views::View);
END_METADATA

}  // namespace

namespace video_conference {

ToggleEffectsView::ToggleEffectsView(VideoConferenceTrayController* controller,
                                     const int parent_width) {
  SetID(BubbleViewID::kToggleEffectsView);

  // Layout for the entire toggle effects section.
  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  // The effects manager provides the toggle effects in rows.
  const VideoConferenceTrayEffectsManager::EffectDataTable tile_rows =
      controller->effects_manager().GetToggleEffectButtonTable();
  for (auto& row : tile_rows) {
    // Each row is its own view, with its own layout.
    std::unique_ptr<views::View> row_view = std::make_unique<views::View>();
    std::unique_ptr<views::FlexLayout> row_layout =
        std::make_unique<views::FlexLayout>();
    row_layout->SetOrientation(views::LayoutOrientation::kHorizontal);
    row_layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    row_layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
    row_view->SetLayoutManager(std::move(row_layout));

    // All buttons in a single row should have the same width i.e. fraction of
    // the parent width.
    const int button_width = parent_width / row.size();

    // Add a button for each item in the row.
    for (auto* tile : row) {
      DCHECK_EQ(tile->type(), VcEffectType::kToggle);
      DCHECK_EQ(tile->GetNumStates(), 1);

      // If `current_state` has no value, it means the state of the effect
      // (represented by `tile`) cannot be obtained. This can happen if the
      // `VcEffectsDelegate` hosting the effect has encountered an error or is
      // in some bad state. In this case its controls are not presented.
      absl::optional<int> current_state = tile->get_state_callback().Run();
      if (!current_state.has_value()) {
        continue;
      }

      // `current_state` can only be a `bool` for a toggle effect.
      bool toggle_state = current_state.value() != 0;
      const VcEffectState* state = tile->GetState(/*index=*/0);
      row_view->AddChildView(std::make_unique<ButtonContainer>(
          state->button_callback(), state->icon(), toggle_state,
          state->label_text(), state->accessible_name_id(), button_width));
    }

    // Add the row as a child, now that it's fully populated,
    AddChildView(std::move(row_view));
  }
}

BEGIN_METADATA(ToggleEffectsView, views::View);
END_METADATA

}  // namespace video_conference

}  // namespace ash