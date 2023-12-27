// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_SESSION_GUEST_SESSION_CONFIRMATION_DIALOG_H_
#define ASH_SYSTEM_SESSION_GUEST_SESSION_CONFIRMATION_DIALOG_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {
class DialogModel;
}

namespace ash {

// This dialog explains to the user how to launch the guest mode from the login
// screen (e.g. by clicking "Browse As Guest") and asks for confirmation to log
// out the user. If the user confirms, they will be sent back to the login
// screen.
class ASH_EXPORT GuestSessionConfirmationDialog {
 public:
  static void Show();

  GuestSessionConfirmationDialog(GuestSessionConfirmationDialog&) = delete;
  GuestSessionConfirmationDialog& operator=(GuestSessionConfirmationDialog&) =
      delete;

 private:
  GuestSessionConfirmationDialog();
  ~GuestSessionConfirmationDialog();

  // Invoked when "ok" button is clicked.
  void OnConfirm();

  // Invoked when the dialog is closing.
  void OnDialogClosing();

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kGuestSessionConfirmationDialogId);
  static GuestSessionConfirmationDialog* g_dialog_;

  raw_ptr<ui::DialogModel> dialog_model_ = nullptr;
  bool should_logout_ = false;

  base::WeakPtrFactory<GuestSessionConfirmationDialog> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_SESSION_GUEST_SESSION_CONFIRMATION_DIALOG_H_
