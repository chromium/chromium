// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_POLICY_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_POLICY_CHECKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"
#include "chrome/common/actor/task_id.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

class GURL;
class Profile;

namespace signin {
class PrimaryAccountChangeEvent;
}  // namespace signin

namespace tabs {
class TabInterface;
}

namespace actor {

class ActorKeyedService;
class AggregatedJournal;

// The central hub for checking various policies that determine whether Actor is
// enabled for the profile, or is Actor allowed to act on a given tab or URL.
class ActorPolicyChecker : public signin::IdentityManager::Observer,
                           public subscription_eligibility::
                               SubscriptionEligibilityService::Observer {
 public:
  explicit ActorPolicyChecker(ActorKeyedService& service);
  ActorPolicyChecker(const ActorPolicyChecker&) = delete;
  ActorPolicyChecker& operator=(const ActorPolicyChecker&) = delete;
  ~ActorPolicyChecker() override;

  static const base::flat_set<int32_t>& GetActorEligibleTiers();

  // `signin::IdentityManager::Observer`:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // `subscription_eligibility::SubscriptionEligibilityService::Observer`:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

  // See site_policy.h.
  void MayActOnTab(const tabs::TabInterface& tab,
                   AggregatedJournal& journal,
                   TaskId task_id,
                   const absl::flat_hash_set<url::Origin>& allowed_origins,
                   DecisionCallbackWithReason callback);
  void MayActOnUrl(const GURL& url,
                   bool allow_insecure_http,
                   Profile* profile,
                   AggregatedJournal& journal,
                   TaskId task_id,
                   DecisionCallbackWithReason callback);

  void SetActOnWebForTesting(bool enabled) {
    can_act_on_web_for_testing_ = enabled;
  }

  // Allows the test to by-pass the enterprise account checking completely.
  void set_account_eligible_for_actuation_for_testing(bool enabled) {
    account_eligible_for_actuation_for_testing_ = enabled;
  }

  bool can_act_on_web() const {
    return can_act_on_web_for_testing_ || can_act_on_web_;
  }

 private:
  void OnPrefOrAccountChanged();

  bool ComputeActOnWebCapability();

  // Owns `this`.
  base::raw_ref<ActorKeyedService> service_;

  PrefChangeRegistrar pref_change_registrar_;

  bool can_act_on_web_ = true;

  bool can_act_on_web_for_testing_ = false;

  bool account_eligible_for_actuation_for_testing_ = false;

  base::SafeRef<AggregatedJournal> journal_;

  // Gets notified when the primary account changes.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Gets notified when the subscription tier changes.
  base::ScopedObservation<
      subscription_eligibility::SubscriptionEligibilityService,
      subscription_eligibility::SubscriptionEligibilityService::Observer>
      subscription_eligibility_service_observation_{this};

  base::WeakPtrFactory<ActorPolicyChecker> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_POLICY_CHECKER_H_
