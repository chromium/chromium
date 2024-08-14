// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_IMPL_H_
#define ASH_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "ash/auth/active_session_auth_metrics_recorder.h"
#include "ash/auth/views/active_session_auth_view.h"
#include "ash/auth/views/auth_common.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/in_session_auth_token_provider.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/login/auth/auth_factor_editor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "components/account_id/account_id.h"
#include "ui/aura/window.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace ash {

// ActiveSessionAuthControllerImpl is responsible for :
// - Initialize the ActiveSessionAuthView and control this view.
// - Create and manage a widget to show the ActiveSessionAuthView.
// - Listening to the ActiveSessionAuthView observers and call auth performer if
// authentication is requested on the UI.
// - Call the callback with the authentication result.
class ASH_EXPORT ActiveSessionAuthControllerImpl
    : public ActiveSessionAuthController,
      public ActiveSessionAuthView::Observer,
      public views::ViewObserver,
      public InSessionAuthTokenProvider {
 public:
  class TestApi {
   public:
    explicit TestApi(ActiveSessionAuthControllerImpl* controller);
    ~TestApi();
    TestApi(const TestApi&) = delete;
    TestApi& operator=(const TestApi&) = delete;

    // Returns the known-to-be-available factors that `ActiveSessionAuthView`
    // was rendered with.
    AuthFactorSet GetAvailableFactors() const;

    // Simulates submitting the `password` to cryptohome as if the user
    // manually entered it.
    void SubmitPassword(const std::string& password);

    // Simulates submitting the `pin` to cryptohome as if the user
    // manually entered it.
    void SubmitPin(const std::string& pin);

    void Close();

   private:
    const raw_ptr<ActiveSessionAuthControllerImpl> controller_;
  };

  ActiveSessionAuthControllerImpl();
  ActiveSessionAuthControllerImpl(const ActiveSessionAuthControllerImpl&) =
      delete;
  ActiveSessionAuthControllerImpl& operator=(
      const ActiveSessionAuthControllerImpl&) = delete;

  ~ActiveSessionAuthControllerImpl() override;

  // ActiveSessionAuthController:
  bool ShowAuthDialog(Reason reason,
                      AuthCompletionCallback on_auth_complete) override;
  bool IsShown() const override;

  // views::ViewObserver:
  void OnViewPreferredSizeChanged(views::View* observed_view) override;

  // ActiveSessionAuthView::Observer:
  void OnPasswordSubmit(const std::u16string& password) override;
  void OnPinSubmit(const std::u16string& pin) override;
  void OnClose() override;

  // InSessionAuthTokenProvider:
  void ExchangeForToken(
      std::unique_ptr<UserContext> user_context,
      InSessionAuthTokenProvider::OnAuthTokenGenerated callback) override;

  // Actions:
  void MoveToTheCenter();
  void Close();

  // Tracks the authentication flow for the active session.
  enum class ActiveSessionAuthState {
    kWaitForInit,            // Initial state, awaiting session start.
    kInitialized,            // Session started, ready for user input.
    kPasswordAuthStarted,    // User submitted password, awaiting verification.
    kPasswordAuthSucceeded,  // Successful password authentication.
    kPinAuthStarted,         // User submitted PIN, awaiting verification.
    kPinAuthSucceeded,       // Successful PIN authentication.
    // Note: On authentication failure, the state reverts to kInitialized.
  };

 private:
  // Set the state of the class, if it necessary disable/enable the input area
  // of the UI. Validates the transitions.
  void SetState(ActiveSessionAuthState state);

  // Internal methods for authentication.
  void OnAuthSessionStarted(
      bool user_exists,
      std::unique_ptr<UserContext> user_context,
      std::optional<AuthenticationError> authentication_error);
  void OnAuthFactorsListed(
      base::OnceClosure callback,
      std::unique_ptr<UserContext> user_context,
      std::optional<AuthenticationError> authentication_error);
  void OnAuthComplete(AuthInputType input_type,
                      std::unique_ptr<UserContext> user_context,
                      std::optional<AuthenticationError> authentication_error);
  void NotifySuccess(const AuthProofToken& token, base::TimeDelta timeout);

  // Initialize the UI after we retrieve the available auth factors from
  // cryptohome.
  void InitUi();

  // After a failed pin attempt we have to retrieve the updated auth factors
  // configuration to can check the pin is still available or not.
  void OnFailedPinAttempt();

  // Checks the pin factor is still available using user_context_. It shpuld be
  // called after auth factors configuration is updated.
  bool IsPinLocked() const;

  std::unique_ptr<views::Widget> widget_;

  base::ScopedObservation<views::View, ViewObserver> contents_view_observer_{
      this};

  raw_ptr<ActiveSessionAuthView> contents_view_ = nullptr;

  AccountId account_id_;
  std::u16string title_;
  std::u16string description_;

  AuthCompletionCallback on_auth_complete_;

  std::unique_ptr<AuthFactorEditor> auth_factor_editor_;
  std::unique_ptr<AuthPerformer> auth_performer_;

  std::unique_ptr<UserContext> user_context_;

  AuthFactorSet available_factors_;
  ActiveSessionAuthState state_ = ActiveSessionAuthState::kWaitForInit;

  Reason reason_;
  ActiveSessionAuthMetricsRecorder uma_recorder_;

  base::WeakPtrFactory<ActiveSessionAuthControllerImpl> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_AUTH_ACTIVE_SESSION_AUTH_CONTROLLER_IMPL_H_
