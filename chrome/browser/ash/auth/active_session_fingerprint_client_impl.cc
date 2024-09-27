// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/auth/active_session_fingerprint_client_impl.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/auth/active_session_auth_controller.h"
#include "ash/public/cpp/auth/active_session_fingerprint_client.h"
#include "ash/public/cpp/login_types.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/osauth/public/request/auth_request.h"
#include "components/account_id/account_id.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

const char kFactorsOptionAll[] = "all";
const char kFactorsOptionFingerprint[] = "FINGERPRINT";

}  // namespace

ActiveSessionFingerprintClientImpl::ActiveSessionFingerprintClientImpl()
    : auth_performer_(UserDataAuthClient::Get()) {
  if (Shell::HasInstance()) {
    Shell::Get()->active_session_auth_controller()->SetFingerprintClient(this);
    shell_observation_.Observe(ash::Shell::Get());
  } else {
    CHECK_IS_TEST();
  }
}

ActiveSessionFingerprintClientImpl::~ActiveSessionFingerprintClientImpl() {
  if (Shell::HasInstance()) {
    Shell::Get()->active_session_auth_controller()->SetFingerprintClient(
        nullptr);
  }
}

void ActiveSessionFingerprintClientImpl::OnShellDestroying() {
  Shell::Get()->active_session_auth_controller()->SetFingerprintClient(nullptr);
  shell_observation_.Reset();
}

bool ActiveSessionFingerprintClientImpl::IsFingerprintAvailable(
    AuthRequest::Reason reason,
    const AccountId& account_id) {
  auto* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);

  CHECK(profile);

  auto* pref_service = profile->GetPrefs();

  if (!pref_service) {
    return false;
  }

  if (profile != ProfileManager::GetPrimaryUserProfile()) {
    return false;
  }

  bool has_record =
      pref_service->GetInteger(prefs::kQuickUnlockFingerprintRecord);

  if (!has_record) {
    return false;
  }

  switch (reason) {
    case AuthRequest::Reason::kSettings:
      return false;
    case AuthRequest::Reason::kPasswordManager: {
      if (pref_service->GetBoolean(
              password_manager::prefs::kBiometricAuthenticationBeforeFilling)) {
        const base::Value::List& factors =
            pref_service->GetList(prefs::kQuickUnlockModeAllowlist);
        if (base::Contains(factors, base::Value(kFactorsOptionAll)) ||
            base::Contains(factors, base::Value(kFactorsOptionFingerprint))) {
          return true;
        }
      }
      return false;
    }
    case AuthRequest::Reason::kWebAuthN: {
      const base::Value::List& factors =
          pref_service->GetList(prefs::kWebAuthnFactors);
      if (base::Contains(factors, base::Value(kFactorsOptionAll)) ||
          base::Contains(factors, base::Value(kFactorsOptionFingerprint))) {
        return true;
      }
      return false;
    }
  }
  NOTREACHED();
}

void ActiveSessionFingerprintClientImpl::PrepareFingerprintAuth(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback auth_ready_callback,
    ActiveSessionFingerprintScanCallback on_scan_callback) {
  CHECK(on_scan_callback_.is_null());
  CHECK(!on_scan_callback.is_null());
  on_scan_callback_ = std::move(on_scan_callback);
  auth_performer_.PrepareAuthFactor(
      std::move(user_context), cryptohome::AuthFactorType::kLegacyFingerprint,
      base::BindOnce(
          &ActiveSessionFingerprintClientImpl::OnPrepareFingerprintAuth,
          weak_factory_.GetWeakPtr(), std::move(auth_ready_callback)));
}

void ActiveSessionFingerprintClientImpl::OnPrepareFingerprintAuth(
    AuthOperationCallback auth_ready_callback,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> auth_error) {
  if (auth_error.has_value()) {
    LOG(ERROR) << "PrepareFingerprintAuth failed.";
    on_scan_callback_.Reset();
  }
  UserDataAuthClient::Get()->AddFingerprintAuthObserver(this);
  std::move(auth_ready_callback).Run(std::move(user_context), auth_error);
}

void ActiveSessionFingerprintClientImpl::TerminateFingerprintAuth(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  UserDataAuthClient::Get()->RemoveFingerprintAuthObserver(this);
  on_scan_callback_.Reset();
  auth_performer_.TerminateAuthFactor(
      std::move(user_context), cryptohome::AuthFactorType::kLegacyFingerprint,
      std::move(callback));
}

void ActiveSessionFingerprintClientImpl::OnFingerprintScan(
    const ::user_data_auth::FingerprintScanResult& result) {
  if (on_scan_callback_.is_null()) {
    return;
  }

  FingerprintAuthScanResult state;
  switch (result) {
    case ::user_data_auth::FingerprintScanResult::
        FINGERPRINT_SCAN_RESULT_SUCCESS:
      state = ash::FingerprintAuthScanResult::kSuccess;
      break;
    case ::user_data_auth::FingerprintScanResult::FINGERPRINT_SCAN_RESULT_RETRY:
      state = ash::FingerprintAuthScanResult::kFailed;
      break;
    case ::user_data_auth::FingerprintScanResult::
        FINGERPRINT_SCAN_RESULT_LOCKOUT:
      state = ash::FingerprintAuthScanResult::kTooManyAttempts;
      break;
    case ::user_data_auth::FingerprintScanResult::
        FINGERPRINT_SCAN_RESULT_FATAL_ERROR:
      state = ash::FingerprintAuthScanResult::kFatalError;
      break;
    default:  // The remaining states are for enrollment.
      NOTREACHED();
  }
  on_scan_callback_.Run(state);
}

void ActiveSessionFingerprintClientImpl::OnEnrollScanDone(
    const ::user_data_auth::FingerprintScanResult& result,
    bool is_complete,
    int percent_complete) {
  CHECK(on_scan_callback_.is_null());
}

}  // namespace ash
