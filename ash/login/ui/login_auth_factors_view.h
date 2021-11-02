// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_

#include <memory>

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

    // Accessor methods for getting references to private members of
    // LoginAuthFactorsView.
    std::vector<std::unique_ptr<AuthFactorModel>>& auth_factors();
    views::Label* label();
    views::View* auth_factor_icon_row();
    ArrowButtonView* arrow_button();
    AuthIconView* checkmark_icon();

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

  void ShowArrowButton();
  void ShowSingleAuthFactor(AuthFactorModel* auth_factor);
  void ShowReadyAuthFactors();
  void ShowCheckmark();

  // Sets the text and accessible name of the label using the provided string
  // IDs.
  void SetLabelTextAndAccessibleName(int label_id, int accessible_name_id);

  // Computes the label to be shown when one or more auth factors are in the
  // Ready state.
  int GetReadyLabelId() const;

  // Causes screen readers to read the label as an alert.
  void FireAlert();

  // Should be called when the "click to enter" button is pressed.
  void ArrowButtonPressed(const ui::Event& event);

  /////////////////////////////////////////////////////////////////////////////
  // Child views, owned by the Views hierarchy

  // A container laying added icons horizontally.
  views::View* auth_factor_icon_row_;

  // The label shown under the icons. Always visible.
  AuthFactorsLabel* label_;

  // A button with an arrow icon. Only visible when an auth factor is in the
  // kClickRequired state.
  ArrowButtonView* arrow_button_;

  // A green checkmark icon (or animation) shown when an auth factor reaches
  // the kAuthenticated state, just before the login/lock screen is dismissed.
  AuthIconView* checkmark_icon_;

  /////////////////////////////////////////////////////////////////////////////

  // The auth factor models that have been added by calling AddAuthFactor().
  // The order here should match the order in which they appear in the UI when
  // multiple are visible.
  std::vector<std::unique_ptr<AuthFactorModel>> auth_factors_;

  base::RepeatingClosure on_click_to_enter_callback_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
