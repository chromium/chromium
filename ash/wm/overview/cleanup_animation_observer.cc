// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/cleanup_animation_observer.h"

#include <utility>

#include "ash/wm/overview/overview_delegate.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Widget must own NativeWidget.
// |widget_| will delete the NativeWidget and NativeWindow.
// Mark the window as not owned by the parent to ensure that
// OnImplicitAnimationsCompleted() is not called from ~Window() and the window
// is not deleted there.
CleanupAnimationObserver::CleanupAnimationObserver(
    std::unique_ptr<views::Widget> widget)
    : widget_(std::move(widget)), owner_(nullptr) {
  DCHECK(widget_);
  widget_->GetNativeWindow()->set_owned_by_parent(false);
}

CleanupAnimationObserver::~CleanupAnimationObserver() = default;

void CleanupAnimationObserver::OnImplicitAnimationsCompleted() {
  // |widget_| may get reset if Shutdown() is called prior to this method.
  if (!widget_)
    return;
  if (owner_) {
    owner_->RemoveAndDestroyExitAnimationObserver(this);
    return;
  }
  delete this;
}

void CleanupAnimationObserver::SetOwner(OverviewDelegate* owner) {
  owner_ = owner;
}

void CleanupAnimationObserver::Shutdown() {
  widget_.reset();
  owner_ = nullptr;
}

}  // namespace ash
