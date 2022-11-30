// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/view_drawn_waiter.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {
namespace {

bool IsDrawn(views::View* view) {
  return view->IsDrawn() && !view->size().IsEmpty();
}

}  // namespace

ViewDrawnWaiter::ViewDrawnWaiter() = default;

ViewDrawnWaiter::~ViewDrawnWaiter() = default;

void ViewDrawnWaiter::Wait(views::View* view) {
  if (IsDrawn(view))
    return;

  DCHECK(!wait_loop_);
  DCHECK(!view_observer_.IsObserving());

  view_observer_.Observe(view);

  wait_loop_ = std::make_unique<base::RunLoop>();
  wait_loop_->Run();
  wait_loop_.reset();

  view_observer_.Reset();
}

void ViewDrawnWaiter::OnViewVisibilityChanged(views::View* view,
                                              views::View* starting_view) {
  if (IsDrawn(view))
    wait_loop_->Quit();
}

void ViewDrawnWaiter::OnViewBoundsChanged(views::View* view) {
  if (IsDrawn(view))
    wait_loop_->Quit();
}

void ViewDrawnWaiter::OnViewIsDeleting(views::View* view) {
  NOTREACHED() << "View deleted while waiting for it to be visible";
}

}  // namespace ash
