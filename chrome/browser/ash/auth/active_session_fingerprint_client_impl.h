// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_AUTH_ACTIVE_SESSION_FINGERPRINT_CLIENT_IMPL_H_
#define CHROME_BROWSER_ASH_AUTH_ACTIVE_SESSION_FINGERPRINT_CLIENT_IMPL_H_

#include <memory>
#include <optional>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/auth/active_session_fingerprint_client.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"
#include "components/account_id/account_id.h"

namespace ash {

class ActiveSessionFingerprintClientImpl
    : public ActiveSessionFingerprintClient,
      public ash::ShellObserver,
      public UserDataAuthClient::FingerprintAuthObserver {
 public:
  ActiveSessionFingerprintClientImpl();
  ActiveSessionFingerprintClientImpl(
      const ActiveSessionFingerprintClientImpl&) = delete;
  ActiveSessionFingerprintClientImpl& operator=(
      const ActiveSessionFingerprintClientImpl&) = delete;
  ~ActiveSessionFingerprintClientImpl() override;

  // ash::ActiveSessionFingerprintClient:
  bool IsFingerprintAvailable(AuthRequest::Reason reason,
                              const AccountId& account_id) override;
  void PrepareFingerprintAuth(
      std::unique_ptr<UserContext> user_context,
      AuthOperationCallback auth_ready_callback,
      ActiveSessionFingerprintScanCallback on_scan_callback) override;

  void TerminateFingerprintAuth(std::unique_ptr<UserContext> user_context,
                                AuthOperationCallback callback) override;

  // ash::ShellObserver:
  void OnShellDestroying() override;

  // ash::UserDataAuthClient::FingerprintAuthObserver:
  void OnFingerprintScan(
      const ::user_data_auth::FingerprintScanResult& result) override;

  void OnEnrollScanDone(const ::user_data_auth::FingerprintScanResult& result,
                        bool is_complete,
                        int percent_complete) override;

 private:
  void OnPrepareFingerprintAuth(AuthOperationCallback auth_ready_callback,
                                std::unique_ptr<UserContext> user_context,
                                std::optional<AuthenticationError> auth_error);

  ActiveSessionFingerprintScanCallback on_scan_callback_;

  AuthPerformer auth_performer_;

  base::ScopedObservation<ash::Shell, ash::ShellObserver> shell_observation_{
      this};

  base::WeakPtrFactory<ActiveSessionFingerprintClientImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_AUTH_ACTIVE_SESSION_FINGERPRINT_CLIENT_IMPL_H_
