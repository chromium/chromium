// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_AUTH_ERROR_BUBBLE_H_
#define ASH_LOGIN_UI_AUTH_ERROR_BUBBLE_H_

#include "ash/login/ui/login_base_bubble_view.h"
#include "ash/login/ui/login_error_bubble.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class StyledLabel;
class LabelButton;
class View;
}  // namespace views

namespace ash {

class ASH_EXPORT AuthErrorBubble : public LoginErrorBubble {
  METADATA_HEADER(AuthErrorBubble, LoginErrorBubble)
  friend class LockContentsViewTestApi;

 public:
  AuthErrorBubble(const base::RepeatingClosure& on_learn_more_button_clicked,
                  const base::RepeatingClosure& on_recover_button_clicked);
  ~AuthErrorBubble() override;

  void ShowAuthError(base::WeakPtr<views::View> anchor_view,
                     int unlock_attempt,
                     bool authenticated_by_pin,
                     bool is_login_screen);

 private:
  void OnLearnMoreButtonPressed();
  void OnRecoverButtonPressed();

  base::RepeatingClosure on_learn_more_button_pressed_;
  base::RepeatingClosure on_recover_button_pressed_;

  raw_ptr<views::StyledLabel> label_;
  raw_ptr<views::LabelButton> learn_more_button_;
  raw_ptr<views::LabelButton> recover_user_button_;

  base::WeakPtrFactory<AuthErrorBubble> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_AUTH_ERROR_BUBBLE_H_
