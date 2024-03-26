// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble.h"

#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/glanceable_tray_bubble_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

GlanceableTrayBubble::GlanceableTrayBubble(DateTray* tray, bool from_keyboard)
    : tray_(tray) {
  TrayBubbleView::InitParams init_params =
      CreateInitParamsForTrayBubble(tray, /*anchor_to_shelf_corner=*/true);
  // TODO(b:277268122): Update with glanceable spec.
  init_params.preferred_width = kWideTrayMenuWidth;
  init_params.transparent = true;
  init_params.has_shadow = false;
  init_params.translucent = false;

  auto bubble_view =
      std::make_unique<GlanceableTrayBubbleView>(init_params, tray_->shelf());
  if (from_keyboard) {
    bubble_view->SetCanActivate(true);
  }
  bubble_wrapper_ = std::make_unique<TrayBubbleWrapper>(tray);
  bubble_wrapper_->ShowBubble(std::move(bubble_view));
}

GlanceableTrayBubble::~GlanceableTrayBubble() {
  bubble_wrapper_->bubble_view()->ResetDelegate();
}

TrayBackgroundView* GlanceableTrayBubble::GetTray() const {
  return tray_;
}

TrayBubbleView* GlanceableTrayBubble::GetBubbleView() const {
  return bubble_wrapper_->bubble_view();
}

views::Widget* GlanceableTrayBubble::GetBubbleWidget() const {
  return bubble_wrapper_->GetBubbleWidget();
}

views::View* GlanceableTrayBubble::GetTasksView() {
  return GetGlanceableTrayBubbleView()->GetTasksView();
}

views::View* GlanceableTrayBubble::GetClassroomStudentView() {
  return GetGlanceableTrayBubbleView()->GetClassroomStudentView();
}

CalendarView* GlanceableTrayBubble::GetCalendarView() {
  return GetGlanceableTrayBubbleView()->GetCalendarView();
}

bool GlanceableTrayBubble::IsBubbleActive() const {
  return GetBubbleWidget()->IsActive();
}

GlanceableTrayBubbleView* GlanceableTrayBubble::GetGlanceableTrayBubbleView() {
  return views::AsViewClass<GlanceableTrayBubbleView>(GetBubbleView());
}

}  // namespace ash
