// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "chrome/browser/ui/views/glic/glic_view.h"

GlicWindowController::GlicWindowController(Profile* profile)
    : profile_(profile) {}

void GlicWindowController::Show() {
  // TODO(crbug.com/379943498): possibly bring to front or activate in this case
  if (widget_) {
    return;
  }

  // TODO(crbug.com/379362838): Determine initial rect based on entrypoint
  widget_ = glic::GlicView::CreateWidget(profile_, {100, 100, 400, 800});
  widget_->Show();
}

void GlicWindowController::Close() {
  if (!widget_) {
    return;
  }

  widget_->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  widget_.reset();
}

base::WeakPtr<GlicWindowController> GlicWindowController::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

GlicWindowController::~GlicWindowController() = default;
