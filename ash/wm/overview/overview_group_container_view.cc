// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_group_container_view.h"

#include "ash/style/style_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_group_item.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kFocusRingRoundedCornerRadius = 20;

}  // namespace

OverviewGroupContainerView::OverviewGroupContainerView(
    OverviewGroupItem* overview_group_item)
    : overview_group_item_(overview_group_item) {
  SetFocusBehavior(FocusBehavior::NEVER);
  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(kFocusRingHaloInset), kFocusRingRoundedCornerRadius);

  views::FocusRing* focus_ring = StyleUtil::SetUpFocusRingForView(this);
  focus_ring->SetHasFocusPredicate(
      base::BindRepeating([](const views::View* view) {
        const auto* v = views::AsViewClass<OverviewGroupContainerView>(view);
        CHECK(v);
        return v->is_focused_;
      }));
}

OverviewGroupContainerView::~OverviewGroupContainerView() = default;

views::View* OverviewGroupContainerView::GetView() {
  return this;
}

void OverviewGroupContainerView::MaybeActivateFocusedView() {
  overview_group_item_->OnFocusedViewActivated();
}

void OverviewGroupContainerView::MaybeCloseFocusedView(bool primary_action) {
  if (primary_action) {
    overview_group_item_->OnFocusedViewClosed();
  }
}

void OverviewGroupContainerView::MaybeSwapFocusedView(bool right) {}

bool OverviewGroupContainerView::MaybeActivateFocusedViewOnOverviewExit(
    OverviewSession* overview_session) {
  return true;
}

void OverviewGroupContainerView::OnFocusableViewFocused() {
  UpdateFocusState(/*focus=*/true);
}

void OverviewGroupContainerView::OnFocusableViewBlurred() {
  UpdateFocusState(/*focus=*/false);
}

void OverviewGroupContainerView::UpdateFocusState(bool focus) {
  if (is_focused_ == focus) {
    return;
  }

  is_focused_ = focus;
  views::FocusRing::Get(this)->SchedulePaint();
}

bool OverviewGroupContainerView::OnMousePressed(const ui::MouseEvent& event) {
  overview_group_item_->HandleMouseEvent(event);
  return true;
}

bool OverviewGroupContainerView::OnMouseDragged(const ui::MouseEvent& event) {
  overview_group_item_->HandleMouseEvent(event);
  return true;
}

void OverviewGroupContainerView::OnMouseReleased(const ui::MouseEvent& event) {
  overview_group_item_->HandleMouseEvent(event);
}

void OverviewGroupContainerView::OnGestureEvent(ui::GestureEvent* event) {
  overview_group_item_->HandleGestureEvent(event);
  event->SetHandled();
}

BEGIN_METADATA(OverviewGroupContainerView, views::View)
END_METADATA

}  // namespace ash
