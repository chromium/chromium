// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_IMPL_H_
#define ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/auth/views/active_session_auth_view.h"
#include "ash/auth/views/auth_common.h"
#include "ash/login/ui/local_authentication_request_view.h"
#include "ash/login/ui/local_authentication_request_widget.h"
#include "ash/public/cpp/login/local_authentication_request_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "components/account_id/account_id.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

class UserContext;
class LocalAuthenticationRequestControllerTestApi;
class LocalAuthenticationWithPinTestApi;

// Implementation of LocalAuthenticationRequestController. It serves to finalize
// the re-auth session with local authentication.
class ASH_EXPORT LocalAuthenticationRequestControllerImpl
    : public LocalAuthenticationRequestController,
      public LocalAuthenticationRequestView::Delegate {
 public:
  friend class LocalAuthenticationRequestControllerTestApi;

  LocalAuthenticationRequestControllerImpl();
  LocalAuthenticationRequestControllerImpl(
      const LocalAuthenticationRequestControllerImpl&) = delete;
  LocalAuthenticationRequestControllerImpl& operator=(
      const LocalAuthenticationRequestControllerImpl&) = delete;
  ~LocalAuthenticationRequestControllerImpl() override;

  // LocalAuthenticationRequestView::Delegate:
  void OnClose() override;

  // LocalAuthenticationRequestController:
  bool ShowWidget(LocalAuthenticationCallback local_authentication_callback,
                  std::unique_ptr<UserContext> user_context) override;
  bool IsDialogVisible() const override;
  bool IsPinSupported() const override;

 private:
  base::WeakPtrFactory<LocalAuthenticationRequestControllerImpl> weak_factory_{
      this};
};

// Implementation of LocalAuthenticationWithPinController. It serves to finalize
// the re-auth session with local authentication (local password or Pin).
class ASH_EXPORT LocalAuthenticationWithPinControllerImpl
    : public LocalAuthenticationRequestController,
      public ActiveSessionAuthView::Observer,
      public UserDataAuthClient::AuthFactorStatusUpdateObserver,
      public views::ViewObserver {
 public:
  friend class LocalAuthenticationWithPinTestApi;

  // Tracks the authentication flow for the active session.
  enum class LocalAuthenticationWithPinState {
    kWaitForInit,            // Initial state, awaiting session start.
    kInitialized,            // Session started, ready for user input.
    kPasswordAuthStarted,    // User submitted password, awaiting verification.
    kPasswordAuthSucceeded,  // Successful password authentication.
    kPinAuthStarted,         // User submitted PIN, awaiting verification.
    kPinAuthSucceeded,       // Successful PIN authentication.
    kCloseRequested,  // Close requested while we are waiting password/PIN
                      // authentication callback.
    // Note: On authentication failure, the state reverts to kInitialized.
  };

  LocalAuthenticationWithPinControllerImpl();
  LocalAuthenticationWithPinControllerImpl(
      const LocalAuthenticationWithPinControllerImpl&) = delete;
  LocalAuthenticationWithPinControllerImpl& operator=(
      const LocalAuthenticationWithPinControllerImpl&) = delete;

  ~LocalAuthenticationWithPinControllerImpl() override;

  // LocalAuthenticationRequestController:
  bool ShowWidget(LocalAuthenticationCallback local_authentication_callback,
                  std::unique_ptr<UserContext> user_context) override;
  bool IsDialogVisible() const override;
  bool IsPinSupported() const override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // ActiveSessionAuthView::Observer:
  void OnPasswordSubmit(const std::u16string& password) override;
  void OnPinSubmit(const std::u16string& pin) override;
  void OnClose() override;

  // UserDataAuthClient::AuthFactorStatusUpdateObserver:
  void OnAuthFactorStatusUpdate(
      const user_data_auth::AuthFactorStatusUpdate& update) override;

  // Actions:
  void MoveToTheCenter();

  void StartClose();

 private:
  // Set the state of the class, if it necessary disable/enable the input area
  // of the UI. Validates the transitions.
  void SetState(LocalAuthenticationWithPinState state);

  bool IsSucceedState() const;

  void OnAuthComplete(AuthInputType input_type,
                      std::unique_ptr<UserContext> user_context,
                      std::optional<AuthenticationError> authentication_error);

  void ProcessAuthFactorStatusUpdate(
      const user_data_auth::AuthFactorStatusUpdate& update);

  std::unique_ptr<views::Widget> widget_;

  base::ScopedObservation<views::View, ViewObserver> contents_view_observer_{
      this};

  raw_ptr<ActiveSessionAuthView> contents_view_ = nullptr;

  AccountId account_id_;
  std::u16string title_;
  std::u16string description_;

  std::string auth_session_broadcast_id_;
  std::u16string pin_status_message_;

  std::unique_ptr<AuthPerformer> auth_performer_;

  std::unique_ptr<UserContext> user_context_;

  AuthFactorSet available_factors_;
  LocalAuthenticationWithPinState state_ =
      LocalAuthenticationWithPinState::kWaitForInit;

  LocalAuthenticationCallback local_authentication_callback_;

  base::WeakPtrFactory<LocalAuthenticationWithPinControllerImpl>
      weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_LOGIN_UI_LOCAL_AUTHENTICATION_REQUEST_CONTROLLER_IMPL_H_
