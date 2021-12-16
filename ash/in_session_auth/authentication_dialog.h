// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IN_SESSION_AUTH_AUTHENTICATION_DIALOG_H_
#define ASH_IN_SESSION_AUTH_AUTHENTICATION_DIALOG_H_

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Textfield;
}

namespace ash {

// To be used for in-session authentication. Currently, only password
// is supported, however, there are plans to enrich this dialog to eventually
// support all configured forms of authentication on the system.
class ASH_EXPORT AuthenticationDialog : public views::DialogDelegateView {
 public:
  enum class Result { kSuccess, kAborted };

  // Callback used to notify invokers of the dialog of success/failure.
  // |timeout| here is the length of time the token retrieved from
  // backends is valid for
  using OnSubmitCallback = base::OnceCallback<void(Result result,
                                                   const std::u16string& token,
                                                   base::TimeDelta timeout)>;

  // Creates and displays a new instance of a widget that hosts the
  // AuthenticationDialog, returning a pointer to it.
  // |submit_callback| is called whenever the "Submit" button is clicked
  static AuthenticationDialog* Show(OnSubmitCallback submit_callback);

  ~AuthenticationDialog() override;

  // Called post widget initialization. For now, this configures the Ok button
  // with custom behavior needed to handle retry of password entry. Also focuses
  // the text input field.
  void Init();

 private:
  explicit AuthenticationDialog(OnSubmitCallback submit_callback);

  void NotifyResult(Result result,
                    const std::u16string& token,
                    base::TimeDelta timeout);

  void ConfigureOkButton();

  void CancelAuthAttempt();

  void OnSubmit();

  void ConfigureChildViews();

  views::Textfield* password_field_;
  views::Label* invalid_password_label_;

  OnSubmitCallback on_submit_;
};

}  // namespace ash

#endif  // ASH_IN_SESSION_AUTH_AUTHENTICATION_DIALOG_H_
