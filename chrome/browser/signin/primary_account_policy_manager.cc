// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/primary_account_policy_manager.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_util.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_pref_names.h"

PrimaryAccountPolicyManager::PrimaryAccountPolicyManager(Profile* profile)
    : profile_(profile) {
  DCHECK(profile_);
  DCHECK(!profile_->IsOffTheRecord());
}

PrimaryAccountPolicyManager::~PrimaryAccountPolicyManager() = default;

void PrimaryAccountPolicyManager::Initialize() {
  signin_util::EnsurePrimaryAccountAllowedForProfile(
      profile_, signin_metrics::SIGNIN_NOT_ALLOWED_ON_PROFILE_INIT);

  signin_allowed_.Init(
      prefs::kSigninAllowed, profile_->GetPrefs(),
      base::BindRepeating(
          &PrimaryAccountPolicyManager::OnSigninAllowedPrefChanged,
          weak_pointer_factory_.GetWeakPtr()));

  local_state_pref_registrar_.Init(g_browser_process->local_state());
  local_state_pref_registrar_.Add(
      prefs::kGoogleServicesUsernamePattern,
      base::BindRepeating(
          &PrimaryAccountPolicyManager::OnGoogleServicesUsernamePatternChanged,
          weak_pointer_factory_.GetWeakPtr()));
}

void PrimaryAccountPolicyManager::Shutdown() {
  local_state_pref_registrar_.RemoveAll();
  signin_allowed_.Destroy();
}

void PrimaryAccountPolicyManager::OnGoogleServicesUsernamePatternChanged() {
  signin_util::EnsurePrimaryAccountAllowedForProfile(
      profile_, signin_metrics::GOOGLE_SERVICE_NAME_PATTERN_CHANGED);
}

void PrimaryAccountPolicyManager::OnSigninAllowedPrefChanged() {
  signin_util::EnsurePrimaryAccountAllowedForProfile(
      profile_, signin_metrics::SIGNOUT_PREF_CHANGED);
}
