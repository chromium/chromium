// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_POLICY_CHECKER_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_POLICY_CHECKER_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/enterprise_policy_url_checker.h"
#include "chrome/browser/subscription_eligibility/subscription_eligibility_service.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "url/origin.h"

class GURL;
class Profile;

namespace signin {
class PrimaryAccountChangeEvent;
}  // namespace signin

namespace actor {
class AggregatedJournal;
}  // namespace actor

namespace glic {

// The Glic implementation of an Actor EnterprisePolicyUrlChecker, used to
// determine the act on web capability enabling state. This class blends various
// signals from account, preferences, managed policies, etc. to make a
// determination.
class GlicActorPolicyChecker : public actor::EnterprisePolicyUrlChecker,
                               public signin::IdentityManager::Observer,
                               public subscription_eligibility::
                                   SubscriptionEligibilityService::Observer {
 public:
  explicit GlicActorPolicyChecker(Profile& profile);
  GlicActorPolicyChecker(const GlicActorPolicyChecker&) = delete;
  GlicActorPolicyChecker& operator=(const GlicActorPolicyChecker&) = delete;
  ~GlicActorPolicyChecker() override;

  static const base::flat_set<int32_t>& GetActorEligibleTiers();

  // Adds a callback to run whenever the value of CanActOnWeb changes.
  using CanActOnWebChangedCallback =
      base::RepeatingCallback<void(bool /*can_act_on_web*/)>;
  base::CallbackListSubscription AddActOnWebCapabilityChangedCallback(
      CanActOnWebChangedCallback callback);

  // `signin::IdentityManager::Observer`:
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnExtendedAccountInfoUpdated(const AccountInfo& info) override;
  void OnExtendedAccountInfoRemoved(const AccountInfo& info) override;

  // `subscription_eligibility::SubscriptionEligibilityService::Observer`:
  void OnAiSubscriptionTierUpdated(int32_t new_subscription_tier) override;

  // Allows tests to synchronize on allow/blocklist updates.
  base::CallbackListSubscription AddUrlListsUpdateObserverForTesting(
      base::RepeatingClosure callback);

  enum class CannotActReason {
    // Browser can actuate.
    kNone,
    // The enterprise policy disables the actuation feature. Only applicable to
    // managed clients (Profile level, browser level or machine level).
    kDisabledByPolicy,
    // The account is not eligible for the actuation.
    kAccountCapabilityIneligible,
    // The account is not subscribed to one of the required AI subscription
    // tiers.
    kAccountMissingChromeBenefits,
    // An enterprise account is logged in but there is no management to deliver
    // the policy. Actuation is disabled because the policy pref default value
    // is disabled.
    kEnterpriseWithoutManagement,
  };

  bool CanActOnWeb() const;
  CannotActReason CannotActOnWebReason() const;

  // EnterprisePolicyUrlChecker interface
  actor::EnterprisePolicyBlockReason Evaluate(const GURL& url) const override;

 private:
  void OnPrefOrAccountChanged();

  enum class CanActOutcome {
    kYes,
    kNo,
    kByAllowlistOnly,
  };
  friend std::ostream& operator<<(std::ostream& os, CanActOutcome value);

  std::pair<CanActOutcome, CannotActReason> ComputeActOnWebCapability();

  // This class must be transitively owned by a Profile and cannot outlive it.
  raw_ptr<Profile> profile_;

  // Client callbacks to run whenever the can_act_on_web_ value changes.
  base::RepeatingCallbackList<CanActOnWebChangedCallback::RunType>
      changed_callback_list_;

  PrefChangeRegistrar pref_change_registrar_;

  CanActOutcome can_act_on_web_ = CanActOutcome::kYes;
  CannotActReason cannot_act_on_web_reason_;

  // Stores enterprise allowlist/blocklist policies for specific URLs.
  policy::URLBlocklistManager url_blocklist_manager_;
  base::CallbackListSubscription url_blocklist_subscription_;

  base::SafeRef<actor::AggregatedJournal> journal_;

  // Gets notified when the primary account changes.
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // Gets notified when the subscription tier changes.
  base::ScopedObservation<
      subscription_eligibility::SubscriptionEligibilityService,
      subscription_eligibility::SubscriptionEligibilityService::Observer>
      subscription_eligibility_service_observation_{this};

  base::WeakPtrFactory<GlicActorPolicyChecker> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_POLICY_CHECKER_H_
