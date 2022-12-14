
// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/toggle_effects_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/icon_button.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

// A single toggle button for a video conference effect, combined with a text
// label.
class ButtonContainer : public views::View {
 public:
  ButtonContainer(views::Button::PressedCallback callback,
                  const gfx::VectorIcon* icon,
                  const std::u16string& label_text,
                  const int accessible_name_id) {
    views::FlexLayout* layout =
        SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical);
    layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

    AddChildView(std::make_unique<views::Label>(label_text));

    std::unique_ptr<IconButton> button = std::make_unique<IconButton>(
        callback, IconButton::Type::kMedium, icon, accessible_name_id,
        /*is_togglable=*/true,
        /*has_border=*/true);
    button->SetID(video_conference::BubbleViewID::kToggleEffectsButton);
    AddChildView(std::move(button));

    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(10, 10)));
    SetBackground(views::CreateRoundedRectBackground(gfx::kGooglePurple800,
                                                     /*radius=*/10));
  }

  ButtonContainer(const ButtonContainer&) = delete;
  ButtonContainer& operator=(const ButtonContainer&) = delete;

  ~ButtonContainer() override = default;
};

}  // namespace

namespace video_conference {

ToggleEffectsView::ToggleEffectsView(
    VideoConferenceTrayController* controller) {
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

    // Add a button for each item in the row.
    for (auto* tile : row) {
      DCHECK_EQ(tile->type(), VcEffectType::kToggle);
      DCHECK_EQ(tile->GetNumStates(), 1);
      const VcEffectState* state = tile->GetState(/*index=*/0);
      row_view->AddChildView(std::make_unique<ButtonContainer>(
          state->button_callback(), state->icon(), state->label_text(),
          state->accessible_name_id()));
    }

    // Add the row as a child, now that it's fully populated,
    AddChildView(std::move(row_view));
  }
}

}  // namespace video_conference

}  // namespace ash