// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/shortcut_removal_dialog.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/native_window_tracker.h"
#include "ui/views/widget/widget.h"

namespace apps {

ShortcutRemovalDialog::ShortcutRemovalDialog(
    Profile* profile,
    const apps::ShortcutId& shortcut_id,
    gfx::NativeWindow parent_window,
    ShortcutRemovalCallback shortcut_removal_callback)
    : profile_(profile),
      shortcut_id_(shortcut_id),
      parent_window_(parent_window),
      shortcut_removal_callback_(std::move(shortcut_removal_callback)) {
  if (parent_window) {
    parent_window_tracker_ = views::NativeWindowTracker::Create(parent_window);
  }
}

ShortcutRemovalDialog::~ShortcutRemovalDialog() = default;

void ShortcutRemovalDialog::CreateDialog(gfx::ImageSkia icon,
                                         gfx::ImageSkia badge_icon) {
  if (parent_window_ && parent_window_tracker_->WasNativeWindowDestroyed()) {
    OnDialogClosed(false);
    return;
  }
  // TODO(crbug.com/1412708): Update this when System UI team provide new
  // interface for the new visual.
  gfx::ImageSkia icon_with_badge =
      gfx::ImageSkiaOperations::CreateIconWithBadge(icon, badge_icon);
  widget_ = Create(profile_, shortcut_id_, icon_with_badge, parent_window_,
                   weak_ptr_factory_.GetWeakPtr());
}

base::WeakPtr<views::Widget> ShortcutRemovalDialog::GetWidget() {
  return widget_;
}

void ShortcutRemovalDialog::OnDialogClosed(bool remove) {
  CHECK(shortcut_removal_callback_);
  std::move(shortcut_removal_callback_).Run(remove, this);
}

void ShortcutRemovalDialog::CloseDialog() {
  if (widget_) {
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
    return;
  }

  OnDialogClosed(false);
}

}  // namespace apps
