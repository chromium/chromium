// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_EDIT_DIALOG_ANDROID_PASSWORD_EDIT_DIALOG_BRIDGE_DELEGATE_H_
#define CHROME_BROWSER_PASSWORD_EDIT_DIALOG_ANDROID_PASSWORD_EDIT_DIALOG_BRIDGE_DELEGATE_H_

#include <memory>

// The delegate for the PasswordEditDialogBridge. Serves like an interface for
// communication between java's password edit dialog side and the
// SaveUpdatePasswordMessageDelegate.
class PasswordEditDialogBridgeDelegate {
 public:
  virtual ~PasswordEditDialogBridgeDelegate() = default;

  // Called by the bridge when the dialog is dismissed.
  virtual void HandleDialogDismissed(bool dialogAccepted) = 0;

  // Called by the bridge when the credenital is saved/updated from the dialog.
  virtual void HandleSavePasswordFromDialog(const std::u16string& username,
                                            const std::u16string& password) = 0;

  // Returns true if the specified credential will be saved/updated in the
  // profile store.
  virtual bool IsUsingAccountStorage(const std::u16string& username) = 0;
};

#endif  // CHROME_BROWSER_PASSWORD_EDIT_DIALOG_ANDROID_PASSWORD_EDIT_DIALOG_BRIDGE_DELEGATE_H_
