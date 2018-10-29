// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_main_view.h"

#include <algorithm>
#include <memory>

#include "ash/assistant/assistant_controller.h"
#include "ash/assistant/assistant_interaction_controller.h"
#include "ash/assistant/assistant_ui_controller.h"
#include "ash/assistant/model/assistant_interaction_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/caption_bar.h"
#include "ash/assistant/ui/dialog_plate/dialog_plate.h"
#include "ash/assistant/ui/main_stage/assistant_main_stage.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Appearance.
constexpr int kMinHeightDip = 200;

// Caption bar animation.
constexpr base::TimeDelta kCaptionBarAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(283);
constexpr base::TimeDelta kCaptionBarAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

// Dialog plate animation.
constexpr base::TimeDelta kDialogPlateAnimationFadeInDelay =
    base::TimeDelta::FromMilliseconds(283);
constexpr base::TimeDelta kDialogPlateAnimationFadeInDuration =
    base::TimeDelta::FromMilliseconds(167);

}  // namespace

AssistantMainView::AssistantMainView(AssistantController* assistant_controller)
    : assistant_controller_(assistant_controller),
      min_height_dip_(kMinHeightDip) {
  InitLayout();

  // Set delegate/observers.
  caption_bar_->set_delegate(assistant_controller_->ui_controller());
  dialog_plate_->AddObserver(assistant_controller_->interaction_controller());
  dialog_plate_->AddObserver(assistant_controller_->ui_controller());

  // The AssistantController indirectly owns the view hierarchy to which
  // AssistantMainView belongs so is guaranteed to outlive it.
  assistant_controller_->ui_controller()->AddModelObserver(this);
}

AssistantMainView::~AssistantMainView() {
  dialog_plate_->RemoveObserver(assistant_controller_->ui_controller());
  dialog_plate_->RemoveObserver(
      assistant_controller_->interaction_controller());

  assistant_controller_->ui_controller()->RemoveModelObserver(this);
}

const char* AssistantMainView::GetClassName() const {
  return "AssistantMainView";
}

gfx::Size AssistantMainView::CalculatePreferredSize() const {
  return gfx::Size(kPreferredWidthDip, GetHeightForWidth(kPreferredWidthDip));
}

int AssistantMainView::GetHeightForWidth(int width) const {
  // |min_height_dip_| <= |height| <= |kMaxHeightDip|.
  int height = views::View::GetHeightForWidth(width);
  height = std::min(height, kMaxHeightDip);
  height = std::max(height, min_height_dip_);

  // |height| should not exceed the height of the usable work area.
  gfx::Rect usable_work_area =
      assistant_controller_->ui_controller()->model()->usable_work_area();
  height = std::min(height, usable_work_area.height());

  return height;
}

void AssistantMainView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  // Until Assistant UI is hidden, the view may grow in height but not shrink.
  min_height_dip_ = std::max(min_height_dip_, height());
}

void AssistantMainView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // Even though the preferred size for |main_stage_| may change, its bounds
  // may not actually change due to height restrictions imposed by its parent.
  // For this reason, we need to explicitly trigger a layout pass so that the
  // children of |main_stage_| are properly updated.
  if (child == main_stage_) {
    Layout();
    SchedulePaint();
  }
}

void AssistantMainView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

views::View* AssistantMainView::FindFirstFocusableView() {
  // In those instances in which we want to override views::FocusSearch
  // behavior, DialogPlate will identify the first focusable view.
  return dialog_plate_->FindFirstFocusableView();
}

void AssistantMainView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Caption bar.
  caption_bar_ = new CaptionBar();
  caption_bar_->SetButtonVisible(CaptionButtonId::kBack, false);

  // The caption bar will be animated on its own layer.
  caption_bar_->SetPaintToLayer();
  caption_bar_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(caption_bar_);

  // Main stage.
  main_stage_ = new AssistantMainStage(assistant_controller_);
  AddChildView(main_stage_);

  layout_manager->SetFlexForView(main_stage_, 1);

  // Dialog plate.
  dialog_plate_ = new DialogPlate(assistant_controller_);

  // The dialog plate will be animated on its own layer.
  dialog_plate_->SetPaintToLayer();
  dialog_plate_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(dialog_plate_);
}

void AssistantMainView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    AssistantSource source) {
  if (assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    // When Assistant is starting a new session, we animate in the appearance of
    // the caption bar and dialog plate.
    using assistant::util::CreateLayerAnimationSequence;
    using assistant::util::CreateOpacityElement;

    // Animate the caption bar from 0% to 100% opacity with delay.
    caption_bar_->layer()->SetOpacity(0.f);
    caption_bar_->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kCaptionBarAnimationFadeInDelay),
            CreateOpacityElement(1.f, kCaptionBarAnimationFadeInDuration)));

    // Animate the dialog plate from 0% to 100% opacity with delay.
    dialog_plate_->layer()->SetOpacity(0.f);
    dialog_plate_->layer()->GetAnimator()->StartAnimation(
        CreateLayerAnimationSequence(
            ui::LayerAnimationElement::CreatePauseElement(
                ui::LayerAnimationElement::AnimatableProperty::OPACITY,
                kDialogPlateAnimationFadeInDelay),
            CreateOpacityElement(1.f, kDialogPlateAnimationFadeInDuration)));

    return;
  }

  if (assistant::util::IsFinishingSession(new_visibility)) {
    // When Assistant is finishing a session, we need to reset view state.
    min_height_dip_ = kMinHeightDip;
    PreferredSizeChanged();
  }
}

void AssistantMainView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

}  // namespace ash
