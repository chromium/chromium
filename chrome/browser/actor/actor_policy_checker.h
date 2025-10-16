// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_POLICY_CHECKER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_POLICY_CHECKER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/actor/task_id.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class Profile;

namespace tabs {
class TabInterface;
}

namespace actor {

class ActorKeyedService;
class AggregatedJournal;

// The central hub for checking various policies that determine whether Actor is
// enabled for the profile, or is Actor allowed to act on a given tab or URL.
class ActorPolicyChecker {
 public:
  explicit ActorPolicyChecker(ActorKeyedService& service);
  ActorPolicyChecker(const ActorPolicyChecker&) = delete;
  ActorPolicyChecker& operator=(const ActorPolicyChecker&) = delete;
  ~ActorPolicyChecker();

  // TODO(crbug.com/448384918): The callback should return the explicit error
  // code to distinguish between different blocked-by-policy reasons.
  using DecisionCallback = base::OnceCallback<void(/*may_act=*/bool)>;
  // See site_policy.h.
  void MayActOnTab(const tabs::TabInterface& tab,
                   AggregatedJournal& journal,
                   TaskId task_id,
                   DecisionCallback callback);
  void MayActOnUrl(const GURL& url,
                   bool allow_insecure_http,
                   Profile* profile,
                   AggregatedJournal& journal,
                   TaskId task_id,
                   DecisionCallback callback);

  bool has_actuation_capability() const { return has_actuation_capability_; }

 private:
  void OnPrefChanged();

  // Owns `this`.
  base::raw_ref<ActorKeyedService> service_;

  PrefChangeRegistrar pref_change_registrar_;

  bool has_actuation_capability_ = true;

  base::WeakPtrFactory<ActorPolicyChecker> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_POLICY_CHECKER_H_
