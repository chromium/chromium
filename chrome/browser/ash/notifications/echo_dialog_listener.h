// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NOTIFICATIONS_ECHO_DIALOG_LISTENER_H_
#define CHROME_BROWSER_ASH_NOTIFICATIONS_ECHO_DIALOG_LISTENER_H_

namespace ash {

// A listener interface for the EchoDialog, so an interested party can be
// notified about changes to the dialog. It is provided during EchoDialog
// construction.
class EchoDialogListener {
 public:
  // Called when the EchoDialog is accepted. After call to this method, the
  // listener will not be invoked again.
  virtual void OnAccept() = 0;

  // Called when the EchoDialog is canceled. After call to this method, the
  // listener will not be invoked again.
  virtual void OnCancel() = 0;

  // Called when a link in the EchoDialog is clicked.
  virtual void OnMoreInfoLinkClicked() = 0;

 protected:
  virtual ~EchoDialogListener() {}
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NOTIFICATIONS_ECHO_DIALOG_LISTENER_H_
