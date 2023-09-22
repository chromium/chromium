// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_SHORTCUT_REMOVAL_DIALOG_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_SHORTCUT_REMOVAL_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget.h"

class Profile;

namespace gfx {
class ImageSkia;
}

namespace views {
class NativeWindowTracker;
}

namespace apps {

class ShortcutRemovalDialog {
 public:
  // Called when the dialog closes after the user has made a decision about
  // whether to remove the shortcut.
  using ShortcutRemovalCallback =
      base::OnceCallback<void(bool remove,
                              ShortcutRemovalDialog* shortcut_removal_dialog)>;

  ShortcutRemovalDialog(Profile* profile,
                        const apps::ShortcutId& shortcut_id,
                        gfx::NativeWindow parent_window,
                        ShortcutRemovalCallback shortcut_removal_callback);
  ShortcutRemovalDialog(const ShortcutRemovalDialog&) = delete;
  ShortcutRemovalDialog& operator=(const ShortcutRemovalDialog&) = delete;
  ~ShortcutRemovalDialog();

  static base::WeakPtr<views::Widget> Create(
      Profile* profile,
      const apps::ShortcutId& shortcut_id,
      gfx::ImageSkia icon_with_badge,
      gfx::NativeWindow parent_window,
      base::WeakPtr<apps::ShortcutRemovalDialog> shortcut_removal_dialog);

  // Create shortcut removal dialog.
  void CreateDialog(gfx::ImageSkia icon, gfx::ImageSkia badge_icon);

  base::WeakPtr<views::Widget> GetWidget();

  // Called when the removal dialog is closing to process remove or cancel
  // the remove.
  void OnDialogClosed(bool remove);

  // Closes this dialog if it is open. If the dialog is not open yet because
  // icons are still loading, immediately runs `shortcut_removal_callback_` so
  // that `this` can be deleted.
  void CloseDialog();

 private:
  const raw_ptr<Profile> profile_;
  const apps::ShortcutId shortcut_id_;
  gfx::NativeWindow parent_window_;
  ShortcutRemovalCallback shortcut_removal_callback_;

  base::WeakPtr<views::Widget> widget_ = nullptr;

  // Tracks whether |parent_window_| got destroyed.
  std::unique_ptr<views::NativeWindowTracker> parent_window_tracker_;

  base::WeakPtrFactory<ShortcutRemovalDialog> weak_ptr_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_SHORTCUT_REMOVAL_DIALOG_H_
