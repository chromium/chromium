// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_VIEW_H_
#define ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_VIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/login/ui/access_code_input.h"
#include "ash/login/ui/login_button.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "ash/public/cpp/login_types.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

class UserContext;

// State of the LocalAuthenticationView.
enum class LocalAuthenticationRequestViewState {
  kNormal,
  kError,
};

// The view that allows for input of local authentication to authorize certain
// actions.
class ASH_EXPORT LocalAuthenticationRequestView
    : public views::DialogDelegateView {
 public:
  using OnLocalAuthenticationRequestDone =
      base::OnceCallback<void(bool success)>;

  class Delegate {
   public:
    virtual void OnClose() = 0;

   protected:
    virtual ~Delegate() = default;
  };

  class ASH_EXPORT TestApi {
   public:
    explicit TestApi(LocalAuthenticationRequestView* view);
    ~TestApi();

    LoginButton* close_button();
    views::Label* title_label();
    views::Label* description_label();
    LoginPasswordView* login_password_view();
    views::Textfield* GetInputTextfield() const;
    LocalAuthenticationRequestViewState state() const;

   private:
    raw_ptr<LocalAuthenticationRequestView, ExperimentalAsh> view_;
  };

  // Creates local authentication request view that will enable the user to
  // authenticate with a local authentication.
  LocalAuthenticationRequestView(
      OnLocalAuthenticationCompleted on_local_authentication_completed,
      const std::u16string& title,
      const std::u16string& description,
      Delegate* delegate,
      std::unique_ptr<UserContext> user_context);

  LocalAuthenticationRequestView(const LocalAuthenticationRequestView&) =
      delete;
  LocalAuthenticationRequestView& operator=(
      const LocalAuthenticationRequestView&) = delete;

  ~LocalAuthenticationRequestView() override;

  // views::DialogDelegateView:
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize() const override;
  views::View* GetInitiallyFocusedView() override;
  std::u16string GetAccessibleWindowTitle() const override;

  // Sets whether the user can enter a local authentication. Other buttons
  // (back, submit etc.) are unaffected.
  void SetInputEnabled(bool input_enabled);

  // Clears previously entered local authentication from the input field.
  void ClearInput();

  // Updates state of the view.
  void UpdateState(LocalAuthenticationRequestViewState state,
                   const std::u16string& title,
                   const std::u16string& description);

 private:
  void OnAuthSubmit(bool authenticated_by_pin, const std::u16string& password);

  void OnAuthComplete(std::unique_ptr<UserContext> user_context,
                      absl::optional<AuthenticationError> authentication_error);

  void OnInputTextChanged(bool is_empty);

  void OnVisibilityChanged();

  // Closes the view.
  void OnClose();

  // Updates view's preferred size.
  void UpdatePreferredSize();

  // Callback to close the UI.
  OnLocalAuthenticationCompleted on_local_authentication_completed_ =
      base::NullCallback();

  // Returns the view dimensions.
  gfx::Size GetLocalAuthenticationRequestViewSize() const;

  // Unowned pointer to the delegate. The delegate should outlive this instance.
  raw_ptr<Delegate, ExperimentalAsh> delegate_;

  // Strings as on view construction to enable restoring the original state.
  std::u16string default_title_;
  std::u16string default_description_;

  // Correspononding labels and other UI elements.
  raw_ptr<views::Label, ExperimentalAsh> title_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> description_label_ = nullptr;
  raw_ptr<LoginButton, ExperimentalAsh> close_button_ = nullptr;
  raw_ptr<LoginPasswordView, ExperimentalAsh> login_password_view_ = nullptr;
  std::unique_ptr<SystemShadow> shadow_;

  // Current local authentication state.
  LocalAuthenticationRequestViewState state_ =
      LocalAuthenticationRequestViewState::kNormal;

  AuthPerformer auth_performer_;

  // Current user context.
  std::unique_ptr<UserContext> user_context_;

  base::WeakPtrFactory<LocalAuthenticationRequestView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_VIEW_H_
