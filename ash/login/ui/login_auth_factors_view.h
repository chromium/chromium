// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_

#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AuthIconView;
class AuthFactorModel;

// A view that displays a collection of auth factors to be shown on the lock and
// login screens.
class LoginAuthFactorsView : public views::View {
 public:
  LoginAuthFactorsView();
  LoginAuthFactorsView(LoginAuthFactorsView&) = delete;
  LoginAuthFactorsView& operator=(LoginAuthFactorsView&) = delete;
  ~LoginAuthFactorsView() override;

  // Add an auth factor to be displayed. Auth factors should be added in the
  // order they should be displayed.
  void AddAuthFactor(std::unique_ptr<AuthFactorModel> auth_factor);

  // Recomputes the state and updates the label and icons. Should be called
  // whenever any auth factor's state changes so that those changes can be
  // reflected in the UI.
  void UpdateState();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;

  // TODO(crbug.com/1233614): Many more methods will be added here to facilitate
  // state management, especially after multiple auth factors have been
  // implemented. See go/cros-smartlock-ui-revamp.

 private:
  // TODO(crbug.com/1233614): Replace |icon_| with a collection of icons and
  // animate them with, e.g. an AnimatingLayoutManager.
  AuthIconView* icon_;
  views::Label* label_;
  std::vector<std::unique_ptr<AuthFactorModel>> auth_factors_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
