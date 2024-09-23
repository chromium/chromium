// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_service.h"

namespace ash {
namespace {

const char kFactorsOptionAll[] = "all";
const char kFactorsOptionFingerprint[] = "FINGERPRINT";

bool HasPolicyValue(const PrefService& pref_service,
                    LegacyFingerprintEngine::Purpose purpose,
                    const char* value) {
  const base::Value::List* factors = nullptr;
  switch (purpose) {
    case LegacyFingerprintEngine::Purpose::kUnlock:
      factors = &pref_service.GetList(prefs::kQuickUnlockModeAllowlist);
      break;
    case LegacyFingerprintEngine::Purpose::kWebAuthn:
      factors = &pref_service.GetList(prefs::kWebAuthnFactors);
      break;
    default:
      return false;
  }
  return base::Contains(*factors, base::Value(value));
}

// Check if fingerprint is disabled for a specific purpose (so not including
// kAny) by reading the policy value.
bool IsFingerprintDisabledByPolicySinglePurpose(
    const PrefService& pref_service,
    LegacyFingerprintEngine::Purpose purpose) {
  DCHECK(purpose != LegacyFingerprintEngine::Purpose::kAny);
  const bool enabled =
      HasPolicyValue(pref_service, purpose, kFactorsOptionAll) ||
      HasPolicyValue(pref_service, purpose, kFactorsOptionFingerprint);
  return !enabled;
}

bool HasRecord(const PrefService& pref_service) {
  return pref_service.GetInteger(prefs::kQuickUnlockFingerprintRecord) != 0;
}

}  // namespace

LegacyFingerprintEngine::LegacyFingerprintEngine(AuthPerformer* auth_performer)
    : auth_performer_(auth_performer) {}

LegacyFingerprintEngine::~LegacyFingerprintEngine() = default;

LegacyFingerprintEngine::Purpose
LegacyFingerprintEngine::FromQuickUnlockPurpose(
    quick_unlock::Purpose purpose) const {
  switch (purpose) {
    case quick_unlock::Purpose::kAny:
      return LegacyFingerprintEngine::Purpose::kAny;
    case quick_unlock::Purpose::kUnlock:
      return LegacyFingerprintEngine::Purpose::kUnlock;
    case quick_unlock::Purpose::kWebAuthn:
      return LegacyFingerprintEngine::Purpose::kWebAuthn;
    case quick_unlock::Purpose::kNumOfPurposes:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return LegacyFingerprintEngine::Purpose::kAny;
}

quick_unlock::Purpose LegacyFingerprintEngine::ToQuickUnlockPurpose(
    LegacyFingerprintEngine::Purpose purpose) const {
  switch (purpose) {
    case LegacyFingerprintEngine::Purpose::kAny:
      return quick_unlock::Purpose::kAny;
    case LegacyFingerprintEngine::Purpose::kUnlock:
      return quick_unlock::Purpose::kUnlock;
    case LegacyFingerprintEngine::Purpose::kWebAuthn:
      return quick_unlock::Purpose::kWebAuthn;
  }
  NOTREACHED_IN_MIGRATION();
  return quick_unlock::Purpose::kAny;
}

bool LegacyFingerprintEngine::IsFingerprintDisabledByPolicy(
    const PrefService& pref_service,
    LegacyFingerprintEngine::Purpose purpose) const {
  // TODO(b/274087315): Create a TestApi for this class.
  if (auto* test_api = quick_unlock::TestApi::Get();
      test_api && test_api->IsQuickUnlockOverridden()) {
    return !test_api->IsFingerprintEnabledByPolicy(
        ToQuickUnlockPurpose(purpose));
  }

  if (purpose == LegacyFingerprintEngine::Purpose::kAny) {
    return IsFingerprintDisabledByPolicySinglePurpose(
               pref_service, LegacyFingerprintEngine::Purpose::kUnlock) &&
           IsFingerprintDisabledByPolicySinglePurpose(
               pref_service, LegacyFingerprintEngine::Purpose::kWebAuthn);
  }
  return IsFingerprintDisabledByPolicySinglePurpose(pref_service, purpose);
}

bool IsFingerprintSupported() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return base::FeatureList::IsEnabled(features::kQuickUnlockFingerprint) &&
         command_line->HasSwitch(switches::kFingerprintSensorLocation);
}

bool LegacyFingerprintEngine::IsFingerprintEnabled(
    const PrefService& pref_service,
    LegacyFingerprintEngine::Purpose purpose) const {
  // TODO(b/274087315): Create a TestApi for this class.
  if (auto* test_api = quick_unlock::TestApi::Get();
      test_api && test_api->IsQuickUnlockOverridden()) {
    // When we override behavior for test we don't need to check
    // `IsFingerprintSupported`
    return !IsFingerprintDisabledByPolicy(pref_service, purpose);
  }

  return !IsFingerprintDisabledByPolicy(pref_service, purpose) &&
         IsFingerprintSupported();
}

bool LegacyFingerprintEngine::IsFingerprintAvailable(
    Purpose purpose,
    const AccountId& account_id) {
  auto* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);

  auto* pref_service = profile->GetPrefs();

  if (!pref_service) {
    return false;
  }

  if (profile != ProfileManager::GetPrimaryUserProfile() ||
      IsFingerprintDisabledByPolicy(*pref_service, purpose)) {
    return false;
  }

  return HasRecord(*pref_service);
}

void LegacyFingerprintEngine::PrepareLegacyFingerprintFactor(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  auth_performer_->PrepareAuthFactor(
      std::move(user_context), cryptohome::AuthFactorType::kLegacyFingerprint,
      std::move(callback));
}

void LegacyFingerprintEngine::TerminateLegacyFingerprintFactor(
    std::unique_ptr<UserContext> user_context,
    AuthOperationCallback callback) {
  auth_performer_->TerminateAuthFactor(
      std::move(user_context), cryptohome::AuthFactorType::kLegacyFingerprint,
      std::move(callback));
}

}  // namespace ash
