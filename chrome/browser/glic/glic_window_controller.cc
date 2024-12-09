// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_window_controller.h"

#include "chrome/browser/ui/views/glic/glic_view.h"
#include "ui/views/controls/webview/webview.h"

namespace glic {

GlicWindowController::GlicWindowController(Profile* profile)
    : profile_(profile) {}

void GlicWindowController::Show() {
  // TODO(crbug.com/379943498): possibly bring to front or activate in this case
  if (widget_) {
    return;
  }

  // TODO(crbug.com/379362838): Determine initial rect based on entrypoint
  std::tie(widget_, glic_view_) =
      glic::GlicView::CreateWidget(profile_, {100, 100, 400, 800});
  widget_->Show();
}

bool GlicWindowController::Resize(const gfx::Size& size) {
  if (!widget_) {
    return false;
  }

  widget_->SetSize(size);
  glic_view_->web_view()->SetSize(size);
  return true;
}

gfx::Size GlicWindowController::GetSize() {
  if (!widget_) {
    return gfx::Size();
  }

  return widget_->GetSize();
}

void GlicWindowController::Close() {
  if (!widget_) {
    return;
  }

  widget_->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
  widget_.reset();
  glic_view_ = nullptr;
}

GlicWindowController::~GlicWindowController() = default;

}  // namespace glic
