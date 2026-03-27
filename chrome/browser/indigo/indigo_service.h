// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_SERVICE_H_
#define CHROME_BROWSER_INDIGO_INDIGO_SERVICE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class PrefChangeRegistrar;
class PrefService;
class Profile;

namespace indigo {

enum class LocalEligibility {
  kEligible,
  kNotSignedIn,
  kMissingCapabilities,
  kDisabledByPolicy,
};

class IndigoService : public KeyedService,
                      public signin::IdentityManager::Observer {
 public:
  using LocalEligibilityChangedCallback =
      base::RepeatingCallback<void(LocalEligibility)>;

  IndigoService(Profile* profile,
                signin::IdentityManager* identity_manager,
                PrefService* pref_service);
  ~IndigoService() override;

  // Get and subscribe to information about whether the profile is eligible to
  // use the feature, as far as Chrome is concerned. This includes the user
  // being signed in with an account which isn't barred, for instance, but not
  // whether the user has actually completed all the steps required to use it.
  //
  // If the profile is not locally eligible, there is no point in offering the
  // feature to users.
  LocalEligibility GetLocalEligibility() const {
    return last_known_local_eligibility_;
  }
  bool IsLocallyEligible() const {
    return GetLocalEligibility() == LocalEligibility::kEligible;
  }
  base::CallbackListSubscription RegisterLocalEligibilityChangedCallback(
      LocalEligibilityChangedCallback callback);

  // Anchored messages are rate-limited to reduce user fatigue. Clients should
  // use `CanShowAnchoredMessage` to check eligibility before displaying an
  // anchored message, and call `AnchoredMessageShown` when they do.
  bool CanShowAnchoredMessage() const;
  void AnchoredMessageShown();

  // KeyedService:
  void Shutdown() override;

  // signin::IdentityManager::Observer:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;

 private:
  LocalEligibility ComputeLocalEligibility() const;
  void UpdateLocalEligibilityAndNotify();

  raw_ptr<Profile> profile_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  raw_ptr<PrefService> pref_service_;

  LocalEligibility last_known_local_eligibility_;
  base::RepeatingCallbackList<void(LocalEligibility)>
      local_eligibility_callback_list_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // The earliest time the anchored message can be shown again.
  base::TimeTicks anchored_message_not_before_;
};

}  // namespace indigo
#endif  // CHROME_BROWSER_INDIGO_INDIGO_SERVICE_H_
