// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_UNIFIED_DEFERRED_UPDATE_DIALOG_H_
#define ASH_SYSTEM_UNIFIED_DEFERRED_UPDATE_DIALOG_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/dialog_model.h"

namespace ash {

class DeferredUpdateDialog {
 public:
  enum Action {
    kShutDown,
    kSignOut,
  };

  DeferredUpdateDialog(DeferredUpdateDialog&) = delete;
  DeferredUpdateDialog& operator=(DeferredUpdateDialog&) = delete;
  virtual ~DeferredUpdateDialog() = default;

  // Shows the deferred update dialog.
  static void CreateDialog(Action callback_action, base::OnceClosure callback);

 private:
  enum DialogResult {
    // Apply the deferred update and enables automatic update.
    kApplyAutoUpdate,
    // Apply the deferred update.
    kApplyUpdate,
    // Ignore the deferred update and run the callback passed in.
    kIgnoreUpdate,
    // Close the dialog without doing anything.
    kClose,
  };

  DeferredUpdateDialog() = default;

  // Invoked when "ok" button is clicked.
  void OnApplyDeferredUpdate();
  // Invoked when "cancel" button is clicked.
  void OnContinueWithoutUpdate();
  // Invoked when the dialog is closing.
  void OnDialogClosing(bool shutdown_after_update, base::OnceClosure callback);

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAutoUpdateCheckboxId);
  static DeferredUpdateDialog* dialog_;

  raw_ptr<ui::DialogModel> dialog_model_ = nullptr;
  DialogResult dialog_result_ = kClose;
};

}  // namespace ash

#endif  // ASH_SYSTEM_UNIFIED_DEFERRED_UPDATE_DIALOG_H_
