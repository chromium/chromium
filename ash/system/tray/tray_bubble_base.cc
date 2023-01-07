// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bubble_base.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/check_op.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

TrayBubbleBase::TrayBubbleBase() = default;
TrayBubbleBase::~TrayBubbleBase() = default;

void TrayBubbleBase::OnWidgetVisibilityChanged(views::Widget* widget,
                                               bool visible) {
  TrayBackgroundView* tray = GetTray();
  views::Widget* current_widget = GetBubbleWidget();
  if (!tray || !current_widget)
    return;
  DCHECK_EQ(current_widget, widget);
  tray->shelf()->GetStatusAreaWidget()->NotifyAnyBubbleVisibilityChanged(
      widget, visible);
}

}  // namespace ash
