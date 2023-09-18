// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/shortcut_removal_dialog.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
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

// TODO(crbug.com/1412708): Add icon loading code.
void ShortcutRemovalDialog::PrepareAndShow() {
  widget_ = Create(profile_, shortcut_id_, parent_window_,
                   weak_ptr_factory_.GetWeakPtr());
}

base::WeakPtr<views::Widget> ShortcutRemovalDialog::GetWidget() {
  return widget_;
}

void ShortcutRemovalDialog::OnDialogClosed(bool remove) {
  CHECK(shortcut_removal_callback_);
  std::move(shortcut_removal_callback_).Run(remove, this);
}

}  // namespace apps
