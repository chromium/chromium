// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_

#include "ash/ash_export.h"
#include "base/callback.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AuthIconView;
class AuthFactorModel;
class AuthFactorsLabel;
class ArrowButtonView;

// A view that displays a collection of auth factors to be shown on the lock and
// login screens.
class ASH_EXPORT LoginAuthFactorsView : public views::View {
 public:
  // TestApi is used for tests to get internal implementation details.
  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LoginAuthFactorsView* view);
    ~TestApi();

    void UpdateState();

    std::vector<std::unique_ptr<AuthFactorModel>>& auth_factors();
    views::Label* label();

   private:
    LoginAuthFactorsView* const view_;
  };

  explicit LoginAuthFactorsView(base::RepeatingClosure on_click_to_enter);
  LoginAuthFactorsView(LoginAuthFactorsView&) = delete;
  LoginAuthFactorsView& operator=(LoginAuthFactorsView&) = delete;
  ~LoginAuthFactorsView() override;

  // Add an auth factor to be displayed. Auth factors should be added in the
  // order they should be displayed.
  void AddAuthFactor(std::unique_ptr<AuthFactorModel> auth_factor);

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnThemeChanged() override;

  // TODO(crbug.com/1233614): Many more methods will be added here to facilitate
  // state management, especially after multiple auth factors have been
  // implemented. See go/cros-smartlock-ui-revamp.

 private:
  // Recomputes the state and updates the label and icons. Should be called
  // whenever any auth factor's state changes so that those changes can be
  // reflected in the UI.
  void UpdateState();

  // Causes screen readers to read the label as an alert.
  void FireAlert();

  // Should be called when the "click to enter" button is pressed.
  void ArrowButtonPressed(const ui::Event& event);

  // TODO(crbug.com/1233614): Replace |icon_| with a collection of icons and
  // animate them with, e.g. an AnimatingLayoutManager.
  AuthIconView* icon_;
  AuthFactorsLabel* label_;
  ArrowButtonView* arrow_button_;
  std::vector<std::unique_ptr<AuthFactorModel>> auth_factors_;

  base::RepeatingClosure on_click_to_enter_callback_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
