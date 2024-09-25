// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/auth/cryptohome_pin_engine.h"

#include "ash/constants/ash_pref_names.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash::legacy {
namespace {

// Possible values for the `kQuickUnlockModeAllowlist` policy.
constexpr char kFactorsOptionAll[] = "all";
constexpr char kFactorsOptionPin[] = "PIN";

bool HasPolicyValue(const PrefService& pref_service,
                    CryptohomePinEngine::Purpose purpose,
                    const char* value) {
  const base::Value::List* factors = nullptr;
  switch (purpose) {
    case CryptohomePinEngine::Purpose::kUnlock:
      factors = &pref_service.GetList(prefs::kQuickUnlockModeAllowlist);
      break;
    case CryptohomePinEngine::Purpose::kWebAuthn:
      factors = &pref_service.GetList(prefs::kWebAuthnFactors);
      break;
    default:
      return false;
  }
  return base::Contains(*factors, base::Value(value));
}

// Check if pin is disabled for a specific purpose (so not including
// kAny) by reading the policy value.
bool IsPinDisabledByPolicySinglePurpose(const PrefService& pref_service,
                                        CryptohomePinEngine::Purpose purpose) {
  DCHECK_NE(purpose, CryptohomePinEngine::Purpose::kAny);
  const bool enabled =
      HasPolicyValue(pref_service, purpose, kFactorsOptionAll) ||
      HasPolicyValue(pref_service, purpose, kFactorsOptionPin);
  return !enabled;
}

// Read the salt from local state.
std::string GetUserSalt(const AccountId& account_id) {
  user_manager::KnownUser known_user(g_browser_process->local_state());
  if (const std::string* salt =
          known_user.FindStringPath(account_id, prefs::kQuickUnlockPinSalt)) {
    return *salt;
  }
  return {};
}

}  // namespace

CryptohomePinEngine::CryptohomePinEngine(ash::AuthPerformer* auth_performer)
    : auth_performer_(auth_performer),
      auth_factor_editor_(ash::UserDataAuthClient::Get()) {}

CryptohomePinEngine::~CryptohomePinEngine() = default;

std::optional<bool> CryptohomePinEngine::IsCryptohomePinDisabledByPolicy(
    const AccountId& account_id,
    CryptohomePinEngine::Purpose purpose) const {
  Profile* profile =
      ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);

  if (!profile) {
    return std::nullopt;
  }

  auto* pref_service = profile->GetPrefs();

  if (!pref_service) {
    return std::nullopt;
  }

  if (purpose == CryptohomePinEngine::Purpose::kAny) {
    return IsPinDisabledByPolicySinglePurpose(
               *pref_service, CryptohomePinEngine::Purpose::kUnlock) &&
           IsPinDisabledByPolicySinglePurpose(
               *pref_service, CryptohomePinEngine::Purpose::kWebAuthn);
  }
  return IsPinDisabledByPolicySinglePurpose(*pref_service, purpose);
}

bool CryptohomePinEngine::ShouldSkipSetupBecauseOfPolicy(
    const AccountId& account_id) const {
  std::optional<bool> is_pin_disabled = IsCryptohomePinDisabledByPolicy(
      account_id, CryptohomePinEngine::Purpose::kAny);
  return is_pin_disabled.value_or(false);
}

void CryptohomePinEngine::IsPinAuthAvailable(
    Purpose purpose,
    std::unique_ptr<UserContext> user_context,
    IsPinAuthAvailableCallback callback) {
  auto is_pin_disabled_by_policy =
      IsCryptohomePinDisabledByPolicy(user_context->GetAccountId(), purpose);

  if (!is_pin_disabled_by_policy.has_value() ||
      is_pin_disabled_by_policy.value()) {
    std::move(callback).Run(false, std::move(user_context));
    return;
  }

  CheckCryptohomePinFactor(std::move(user_context), std::move(callback));
}

void CryptohomePinEngine::Authenticate(
    const cryptohome::RawPin& pin,
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  auto salt = GetUserSalt(user_context->GetAccountId());
  auth_performer_->AuthenticateWithPin(*pin, salt, std::move(user_context),
                                       std::move(callback));
}

void CryptohomePinEngine::CheckCryptohomePinFactor(
    std::unique_ptr<UserContext> user_context,
    IsPinAuthAvailableCallback callback) {
  auth_factor_editor_.GetAuthFactorsConfiguration(
      std::move(user_context),
      base::BindOnce(&CryptohomePinEngine::OnGetAuthFactorsConfiguration,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CryptohomePinEngine::OnGetAuthFactorsConfiguration(
    IsPinAuthAvailableCallback callback,
    std::unique_ptr<UserContext> user_context,
    std::optional<AuthenticationError> error) {
  if (error.has_value()) {
    std::move(callback).Run(false, std::move(user_context));
    return;
  }

  const auto& config = user_context->GetAuthFactorsConfiguration();
  const cryptohome::AuthFactor* pin_factor =
      config.FindFactorByType(cryptohome::AuthFactorType::kPin);

  if (!pin_factor || pin_factor->GetPinStatus().IsLockedFactor()) {
    std::move(callback).Run(false, std::move(user_context));
    return;
  }

  std::move(callback).Run(true, std::move(user_context));
}

}  // namespace ash::legacy
