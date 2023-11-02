// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ime/mode_indicator_observer.h"

#include "ui/views/widget/widget.h"

namespace ash {

ModeIndicatorObserver::ModeIndicatorObserver() : active_widget_(nullptr) {}

ModeIndicatorObserver::~ModeIndicatorObserver() {
  if (active_widget_)
    active_widget_->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void ModeIndicatorObserver::AddModeIndicatorWidget(views::Widget* widget) {
  // If other active mode indicator widget is shown, close it immediately
  // without fading animation.  Then store this widget as the active widget.
  DCHECK(widget);
  if (active_widget_)
    active_widget_->Close();
  active_widget_ = widget;
  widget->AddObserver(this);
}

void ModeIndicatorObserver::OnWidgetDestroying(views::Widget* widget) {
  if (widget == active_widget_)
    active_widget_ = nullptr;
}

}  // namespace ash
