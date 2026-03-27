// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_service.h"

#include "base/functional/bind.h"
#include "chrome/browser/indigo/indigo_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace indigo {

IndigoService::IndigoService(Profile* profile,
                             signin::IdentityManager* identity_manager,
                             PrefService* pref_service)
    : profile_(profile),
      identity_manager_(identity_manager),
      pref_service_(pref_service) {
  if (identity_manager_) {
    identity_manager_observation_.Observe(identity_manager_);
  }

  if (pref_service_) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service_);
    pref_change_registrar_->Add(
        prefs::kIndigoPolicy,
        base::BindRepeating(&IndigoService::UpdateLocalEligibilityAndNotify,
                            base::Unretained(this)));
  }

  last_known_local_eligibility_ = ComputeLocalEligibility();
}

IndigoService::~IndigoService() = default;

void IndigoService::Shutdown() {
  identity_manager_observation_.Reset();
  pref_change_registrar_.reset();
}

void IndigoService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    UpdateLocalEligibilityAndNotify();
  }
}

void IndigoService::OnExtendedAccountInfoUpdated(const AccountInfo& info) {
  UpdateLocalEligibilityAndNotify();
}

base::CallbackListSubscription
IndigoService::RegisterLocalEligibilityChangedCallback(
    LocalEligibilityChangedCallback callback) {
  return local_eligibility_callback_list_.Add(std::move(callback));
}

bool IndigoService::CanShowAnchoredMessage() const {
  return base::TimeTicks::Now() >= anchored_message_not_before_;
}

void IndigoService::AnchoredMessageShown() {
  anchored_message_not_before_ =
      base::TimeTicks::Now() +
      features::kIndigoAnchoredMessageResetDuration.Get();
}

LocalEligibility IndigoService::ComputeLocalEligibility() const {
  if (pref_service_) {
    int policy_val = pref_service_->GetInteger(prefs::kIndigoPolicy);
    if (policy_val != prefs::Policy::kAllowed) {
      return LocalEligibility::kDisabledByPolicy;
    }
  }

  CoreAccountId account_id = identity_manager_
                                 ? identity_manager_->GetPrimaryAccountId(
                                       signin::ConsentLevel::kSignin)
                                 : CoreAccountId();
  if (account_id.empty()) {
    return LocalEligibility::kNotSignedIn;
  }

  AccountInfo info =
      identity_manager_->FindExtendedAccountInfoByAccountId(account_id);
  if (info.capabilities.can_use_model_execution_features() !=
      signin::Tribool::kTrue) {
    return LocalEligibility::kMissingCapabilities;
  }

  return LocalEligibility::kEligible;
}

void IndigoService::UpdateLocalEligibilityAndNotify() {
  LocalEligibility new_eligibility = ComputeLocalEligibility();
  if (new_eligibility != last_known_local_eligibility_) {
    last_known_local_eligibility_ = new_eligibility;
    local_eligibility_callback_list_.Notify(new_eligibility);
  }
}

}  // namespace indigo
