// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overlay_event_filter.h"

#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/events/event.h"
#include "ui/views/widget/widget.h"

namespace ash {

OverlayEventFilter::OverlayEventFilter() : scoped_session_observer_(this) {}

OverlayEventFilter::~OverlayEventFilter() {
  delegate_ = nullptr;
}

void OverlayEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  if (!delegate_)
    return;

  if (delegate_ && delegate_->IsCancelingKeyEvent(event))
    Cancel();

  // Pass key events only when they are sent to a child of the delegate's
  // window.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (!delegate_ || !delegate_->GetWindow() ||
      !delegate_->GetWindow()->Contains(target))
    event->StopPropagation();
}

void OverlayEventFilter::OnLoginStatusChanged(LoginStatus status) {
  Cancel();
}

void OverlayEventFilter::OnChromeTerminating() {
  Cancel();
}

void OverlayEventFilter::OnLockStateChanged(bool locked) {
  Cancel();
}

void OverlayEventFilter::Activate(Delegate* delegate) {
  if (delegate_)
    delegate_->Cancel();
  delegate_ = delegate;
}

void OverlayEventFilter::Deactivate(Delegate* delegate) {
  if (delegate_ == delegate)
    delegate_ = nullptr;
}

void OverlayEventFilter::Cancel() {
  if (delegate_) {
    delegate_->Cancel();
    delegate_ = nullptr;
  }
}

bool OverlayEventFilter::IsActive() {
  return delegate_ != nullptr;
}

}  // namespace ash
