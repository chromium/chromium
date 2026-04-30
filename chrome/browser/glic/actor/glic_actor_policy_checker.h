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
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "components/policy/core/browser/url_list/url_blocklist_manager.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/subscription_eligibility/subscription_eligibility_service.h"
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

// The Glic implementation of the EnterprisePolicyChecker interface, used to
// determine the act on web capability
// enabling state and validate content sent to the renderer. This class blends
// various signals from account, preferences, managed policies, etc. to make a
// determination.
class GlicActorPolicyChecker : public actor::EnterprisePolicyChecker,
                               public signin::IdentityManager::Observer,
                               public subscription_eligibility::
                                   SubscriptionEligibilityService::Observer {
 public:
  explicit GlicActorPolicyChecker(Profile& profile);
  GlicActorPolicyChecker(const GlicActorPolicyChecker&) = delete;
  GlicActorPolicyChecker& operator=(const GlicActorPolicyChecker&) = delete;
  ~GlicActorPolicyChecker() override;

  static const base::flat_set<int32_t>& GetActorEligibleTiers();

  // Returns true if Glic Actor considers the profile to belong to a managed
  // Enterprise account. This check is specific to Glic Actor and should not
  // be used as a generic check for managed Enterprise accounts.
  static bool IsEnterpriseAccount(Profile& profile,
                                  actor::AggregatedJournal& journal);

  // Returns true if the Chrome browser is managed by an IT administrator.
  static bool IsBrowserManaged(Profile& profile);

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

  using CannotActReason = ::glic::CannotActReason;

  bool CanActOnWeb() const;
  CannotActReason CannotActOnWebReason() const;

  // EnterprisePolicyChecker interface
  UrlBlockReason Evaluate(const GURL& url) const override;
  void ValidateContentSentToRenderer(
      content::RenderFrameHost* frame,
      const std::string& content,
      ContentValidationCallback callback) const override;

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
