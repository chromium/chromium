// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/video_conference/bubble/bubble_view.h"

#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/return_to_app_button.h"
#include "ash/system/video_conference/bubble/set_value_effects_view.h"
#include "ash/system/video_conference/bubble/toggle_effects_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager.h"
#include "ash/system/video_conference/video_conference_tray_controller.h"
#include "ui/views/border.h"
#include "ui/views/layout/flex_layout.h"

namespace ash::video_conference {

namespace {

const int kBorderInsetDimension = 10;

}  // namespace

BubbleView::BubbleView(const InitParams& init_params,
                       VideoConferenceTrayController* controller)
    : TrayBubbleView(init_params) {
  SetID(BubbleViewID::kMainBubbleView);

  views::FlexLayout* layout =
      SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout->SetOrientation(views::LayoutOrientation::kVertical);
  layout->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout->SetCrossAxisAlignment(views::LayoutAlignment::kStretch);

  AddChildView(std::make_unique<ReturnToAppButton>());

  if (controller->effects_manager().HasToggleEffects())
    AddChildView(std::make_unique<ToggleEffectsView>(controller));

  if (controller->effects_manager().HasSetValueEffects()) {
    AddChildView(std::make_unique<SetValueEffectsView>(controller));
  }

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kBorderInsetDimension, kBorderInsetDimension)));
}

}  // namespace ash::video_conference
