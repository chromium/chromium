// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
#define ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/login/ui/auth_factor_model.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class AuthIconView;
class AuthFactorModel;
class AnimatedAuthFactorsLabelWrapper;
class ArrowButtonView;

// A view that displays a collection of auth factors to be shown on the lock and
// login screens.
class ASH_EXPORT LoginAuthFactorsView : public views::View {
  METADATA_HEADER(LoginAuthFactorsView, views::View)

 public:
  using AuthFactorState = AuthFactorModel::AuthFactorState;

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
    AuthIconView* arrow_nudge_animation();
    AuthIconView* checkmark_icon();

   private:
    const raw_ptr<LoginAuthFactorsView> view_;
  };

  LoginAuthFactorsView(base::RepeatingClosure on_click_to_enter_callback,
                       base::RepeatingCallback<void(bool)>
                           on_auth_factor_is_hiding_password_changed_callback);
  LoginAuthFactorsView(LoginAuthFactorsView&) = delete;
  LoginAuthFactorsView& operator=(LoginAuthFactorsView&) = delete;
  ~LoginAuthFactorsView() override;

  // Add an auth factor to be displayed. Auth factors should be added in the
  // order they should be displayed.
  void AddAuthFactor(std::unique_ptr<AuthFactorModel> auth_factor);

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // Should be called when the visibility of PIN authentication changes.
  // Used to determine whether strings should mention that PIN can be used as an
  // authentication mechanism.
  void SetCanUsePin(bool can_use_pin);

  // Returns true if the active auth factor is requesting to take visual
  // precedence, by hiding the password field.
  bool ShouldHidePasswordField();

 private:
  // Recomputes the state and updates the label and icons. Should be called
  // whenever any auth factor's state changes so that those changes can be
  // reflected in the UI.
  void UpdateState();

  void ShowArrowButton();
  void ShowSingleAuthFactor(AuthFactorModel* auth_factor);
  void ShowReadyAndDisabledAuthFactors();
  void ShowCheckmark();

  // Computes the label to be shown when one or more auth factors are in the
  // Ready state.
  int GetReadyLabelId() const;

  // Gets the label to be shown when no auth factor can be used.
  int GetDefaultLabelId() const;

  // Causes screen readers to read the label as an alert.
  void FireAlert();

  // Should be called when the "click to enter" button is pressed.
  void ArrowButtonPressed(const ui::Event& event);

  // Used when |arrow_nudge_animation_| is pressed. It prevents arrow button
  // from receiving its click event directly, so it relays the click event.
  void RelayArrowButtonPressed();

  // Should be called when the error timer expires. Communicates the timeout to
  // the auth factor models.
  void OnErrorTimeout();

  // Calls views::View::SetLayoutManager with views::BoxLayout for provided
  // view.
  void SetBoxLayout(views::View* parent_view);

  // Sets visibility of |arrow_icon_container_|, |arrow_button_|, and
  // |arrow_nudge_animation_| and starts/stops arrow animations accordingly.
  void SetArrowVisibility(bool is_visible);

  // Sets |should_hide_password_field_| and invokes
  // |auth_factor_click_changed_callback_| if |should_hide_password_field_| has
  // changed.
  void UpdateShouldHidePasswordField(const AuthFactorModel& active_auth_factor);

  /////////////////////////////////////////////////////////////////////////////
  // Child views, owned by the Views hierarchy

  // A container laying added icons horizontally.
  raw_ptr<views::View> auth_factor_icon_row_;

  // An animated label.
  raw_ptr<AnimatedAuthFactorsLabelWrapper> label_wrapper_;

  // A container laying arrow button and its corresponding animation view on top
  // of each other.
  raw_ptr<views::View> arrow_icon_container_;

  // A box layout container for arrow button and its label.
  raw_ptr<views::View> arrow_button_container_;

  // A button with an arrow icon. Only visible when an auth factor is in the
  // kClickRequired state.
  raw_ptr<ArrowButtonView> arrow_button_;

  // A view with nudge animation expanding from arrow icon to encourage user to
  // tap. Only visible when an auth factor is in the kClickRequired state.
  raw_ptr<AuthIconView> arrow_nudge_animation_;

  // A green checkmark icon (or animation) shown when an auth factor reaches
  // the kAuthenticated state, just before the login/lock screen is dismissed.
  raw_ptr<AuthIconView> checkmark_icon_;

  /////////////////////////////////////////////////////////////////////////////

  // The auth factor models that have been added by calling `AddAuthFactor`.
  // The order here should match the order in which they appear in the UI when
  // multiple are visible.
  std::vector<std::unique_ptr<AuthFactorModel>> auth_factors_;

  // True if an active auth factor is requesting to hide the password field.
  // Changes value based on the current state of the active auth factor.
  bool should_hide_password_field_ = false;

  base::RepeatingClosure on_click_to_enter_callback_;
  base::RepeatingCallback<void(bool)>
      on_auth_factor_is_hiding_password_changed_callback_;
  base::OneShotTimer error_timer_;
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOGIN_AUTH_FACTORS_VIEW_H_
