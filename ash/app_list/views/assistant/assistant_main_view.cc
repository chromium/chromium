// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/assistant/assistant_main_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/views/assistant/assistant_dialog_plate.h"
#include "ash/app_list/views/assistant/assistant_main_stage.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/util/animation_util.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/search_box/search_box_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Dialog plate animation.
constexpr base::TimeDelta kDialogPlateAnimationFadeInDelay =
    base::Milliseconds(283);
constexpr base::TimeDelta kDialogPlateAnimationFadeInDuration =
    base::Milliseconds(167);

}  // namespace

AssistantMainView::AssistantMainView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kMainView);
  InitLayout();

  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantMainView::~AssistantMainView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);
}

void AssistantMainView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();

  // Even though the preferred size for |main_stage_| may change, its bounds
  // may not actually change due to height restrictions imposed by its parent.
  // For this reason, we need to explicitly trigger a layout pass so that the
  // children of |main_stage_| are properly updated.
  if (child == main_stage_) {
    DeprecatedLayoutImmediately();
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

void AssistantMainView::RequestFocus() {
  dialog_plate_->RequestFocus();
}

void AssistantMainView::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void AssistantMainView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  if (!assistant::util::IsStartingSession(new_visibility, old_visibility)) {
    return;
  }

  // When Assistant is starting a new session, we animate in the appearance of
  // the dialog plate.
  using assistant::util::CreateLayerAnimationSequence;
  using assistant::util::CreateOpacityElement;

  // Animate the dialog plate from 0% to 100% opacity with delay.
  dialog_plate_->layer()->SetOpacity(0.f);
  dialog_plate_->layer()->GetAnimator()->StartAnimation(
      CreateLayerAnimationSequence(
          ui::LayerAnimationElement::CreatePauseElement(
              ui::LayerAnimationElement::AnimatableProperty::OPACITY,
              kDialogPlateAnimationFadeInDelay),
          CreateOpacityElement(1.f, kDialogPlateAnimationFadeInDuration)));
}

void AssistantMainView::InitLayout() {
  constexpr int radius = kSearchBoxBorderCornerRadiusSearchResult;

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius({radius, radius, radius, radius});

  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Dialog plate, which will be animated on its own layer.
  dialog_plate_ =
      AddChildView(std::make_unique<AssistantDialogPlate>(delegate_));
  dialog_plate_->SetPaintToLayer();
  dialog_plate_->layer()->SetFillsBoundsOpaquely(false);

  // Main stage.
  main_stage_ =
      AddChildView(std::make_unique<AppListAssistantMainStage>(delegate_));

  layout->SetFlexForView(main_stage_, 1);
}

BEGIN_METADATA(AssistantMainView)
END_METADATA

}  // namespace ash
