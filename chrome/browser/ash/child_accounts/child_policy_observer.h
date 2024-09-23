// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_POLICY_OBSERVER_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_POLICY_OBSERVER_H_

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"

class Profile;

namespace base {
class OneShotTimer;
}  // namespace base

namespace policy {
class UserCloudPolicyManagerAsh;
}  // namespace policy

namespace ash {

// Observes initial policy refresh for child user.
// Unlike for regular user, child user policy is refreshed after profile
// initialization as a part of the refresh schedule. This class tracks initial
// policy refresh and sends notification when it is done.
// Child policy is considered ready if one of the conditions is fulfilled:
// * Policy refresh finished successfully (fresh policy is available)
// * Policy refresh finished with error (cached policy is available)
// * Waiting for policy refresh timed out (cached policy is available)
class ChildPolicyObserver : public policy::CloudPolicyService::Observer {
 public:
  // Result of the initial policy refresh.
  enum class InitialPolicyRefreshResult {
    kUnknown,              // Result unknown.
    kPolicyRefreshed,      // Initial policy refresh finished successfully.
    kPolicyRefreshError,   // Initial policy refresh finished with error.
    kPolicyRefreshTimeout  // Initial policy refresh timed out.
  };

  using PolicyReadyCallback =
      base::OnceCallback<void(Profile*, InitialPolicyRefreshResult)>;

  explicit ChildPolicyObserver(Profile* profile);

  ChildPolicyObserver(const ChildPolicyObserver&) = delete;
  ChildPolicyObserver& operator=(const ChildPolicyObserver&) = delete;

  ~ChildPolicyObserver() override;

  // policy::CloudPolicyService::Observer:
  void OnCloudPolicyServiceInitializationCompleted() override;
  void OnPolicyRefreshed(bool success) override;
  std::string_view name() const override;

  // Requests notification when policy is ready. Passed |on_policy_ready| will
  // be invoked when initial policy refresh is finished. Information about
  // refresh success or error will be passed in the |on_policy_ready|. If policy
  // refresh is not finished before |timeout| the refresh will be considered
  // timed out and |on_policy_ready| will be invoked.
  // If policy refresh is finshed before this method is called,
  // |on_policy_ready| will be invoked immediately.
  void NotifyWhenPolicyReady(PolicyReadyCallback on_policy_ready,
                             base::TimeDelta timeout);

 private:
  // Returns whether initial policy refresh finished and policy is considered
  // ready.
  bool IsChildPolicyReady() const;

  // Called when initial policy refresh finished or timed out.
  void OnPolicyReady(InitialPolicyRefreshResult refresh_result);

  // Returns user cloud policy manager for |profile_|.
  policy::UserCloudPolicyManagerAsh* GetUserCloudPolicyManager();

  // The result of initial policy refresh for child user.
  InitialPolicyRefreshResult refresh_result_ =
      InitialPolicyRefreshResult::kUnknown;

  // Timer that fires to prevent indefinite wait if the refresh takes too long.
  std::unique_ptr<base::OneShotTimer> refresh_timeout_timer_;

  // Callback to be invoked when child policy refresh finshed (successfully,
  // with an error or timed out). Notifies the requester that policy is ready.
  PolicyReadyCallback on_policy_ready_;

  // Profile of the child user, not owned.
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_POLICY_OBSERVER_H_
