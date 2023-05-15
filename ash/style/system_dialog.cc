// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/system_dialog.h"

#include <algorithm>

#include "ash/style/system_dialog_delegate_view.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

// Typical sizes of a dialog.
constexpr int kDialogWidthLarge = 512;
constexpr int kDialogWidthMedium = 359;
constexpr int kDialogWidthSmall = 296;

// The host window sizes that will change the resizing rule of the dialog.
constexpr int kHostWidthLarge = 672;
constexpr int kHostWidthMedium = 520;
constexpr int kHostWidthSmall = 424;
constexpr int kHostWidthXSmall = 400;

// Padding between the dialog and the host window.
constexpr int kDialogHostPaddingLarge = 80;
constexpr int kDialogHostPaddingSmall = 32;

SystemDialog::SystemDialog(
    std::unique_ptr<SystemDialogDelegateView> dialog_view,
    aura::Window* host_window)
    : host_window_(host_window) {
  views::Widget::InitParams params;
  params.type = views::Widget::InitParams::TYPE_WINDOW_FRAMELESS;
  params.parent = host_window_;
  // Initialize the dialog bounds with the delegate view's preferred size so
  // that the sub views could have an appropriate initial layout.
  params.bounds = gfx::Rect(dialog_view->GetPreferredSize());
  params.delegate = dialog_view.release();

  // The widget is owned by its native widget.
  widget_ = new views::Widget(std::move(params));
  UpdateDialogBounds();

  window_observations_.AddObservation(host_window_);
  window_observations_.AddObservation(widget_->GetNativeWindow());

  widget_->Show();
}

SystemDialog::~SystemDialog() {
  if (widget_) {
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void SystemDialog::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  if (window == host_window_) {
    UpdateDialogBounds();
  }
}

void SystemDialog::OnWindowDestroying(aura::Window* window) {
  widget_ = nullptr;
  window_observations_.RemoveAllObservations();
}

void SystemDialog::UpdateDialogBounds() {
  CHECK(widget_);

  const gfx::Rect host_bounds = host_window_->GetBoundsInScreen();
  const int host_width = host_bounds.width();
  // The resizing rules of the dialog are as follows:
  // - When the host window width is larger than `kHostWidthLarge`, the dialog
  // width would remain at `kDialogWidthLarge`.
  // - When the host window width is between `kHostWidthMedium` and
  // `kHostWidthLarge`, the dialog width will decrease but maintain a padding
  // of `kDialogHostPaddingLarge` on both sides.
  // - When the host window width is between `kHostWidthSmall` and
  // `kHostWidthMedium`, the dialog width would remain at `kDialogWidthMedium`.
  // - When the host window width is less than `kHostWidthXSmall`, the dialog
  // width will decrease but maintain a padding of `kDialogHostPaddingSmall` on
  // both sides.
  // - The dialog minimum width is `kDialogWidthSmall`.
  int width = kDialogWidthSmall;
  if (host_width >= kHostWidthLarge) {
    width = kDialogWidthLarge;
  } else if (host_width >= kHostWidthMedium) {
    width = host_width - kDialogHostPaddingLarge * 2;
  } else if (host_width >= kHostWidthSmall) {
    width = kDialogWidthMedium;
  } else if (host_width >= kHostWidthXSmall) {
    width = host_width - kDialogHostPaddingSmall * 2;
  }

  auto* dialog_view = widget_->GetContentsView();
  width = std::clamp(width, dialog_view->GetMinimumSize().width(),
                     dialog_view->GetMaximumSize().width());
  const int height = dialog_view->GetHeightForWidth(width);
  const gfx::Point center = host_bounds.CenterPoint();
  widget_->SetBounds(gfx::Rect(center.x() - width / 2, center.y() - height / 2,
                               width, height));
}

}  // namespace ash
