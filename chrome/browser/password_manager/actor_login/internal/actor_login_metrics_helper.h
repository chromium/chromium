// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_HELPER_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics.h"
#include "components/password_manager/core/browser/actor_login/internal/actor_login_metrics_helper_interface.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace actor_login {

// Helper class for recording Actor.Login metrics such as the number and types
// of accounts shown to the user in an actor login flow. Metrics are recorded in
// the destructor, using the data collected during the lifetime of this object.
class ActorLoginMetricsHelper : public ActorLoginMetricsHelperInterface {
 public:
  explicit ActorLoginMetricsHelper(ukm::SourceId source_id);
  ~ActorLoginMetricsHelper() override;

  ActorLoginMetricsHelper(const ActorLoginMetricsHelper&) = delete;
  ActorLoginMetricsHelper& operator=(const ActorLoginMetricsHelper&) = delete;

  // ActorLoginMetricsHelperInterface:
  void RecordDeduplicationOccurred(bool deduplication_occurred) override;

  // Records the types of accounts (password, federated, both) shown to the
  // user.
  void RecordAccountTypesShown(ActorLoginAccountTypes types);

  // Records the number of accounts shown to the user.
  void RecordNumAccountsShown(int count);

  // Records whether an account was automatically selected due to previously
  // having been selected as 'always allow'.
  void RecordAccountAutoSelected(bool auto_selected);

  // Records the type of account selected by the user (password, federated).
  void RecordSelectedAccountType(ActorLoginSelectedAccountType type);

  // Marks the start of a "get credentials" request. Computes the time at which
  // the request starts.
  void OnGetCredentialsStarted();

  // Marks that a "get credentials" request has completed. Records the duration
  // of the request (end - start).
  void OnGetCredentialsCompleted();

  // Marks that an account has been chosen by the user. Records the time from
  // request start to when the account was chosen.
  void OnAccountChosen();

  // Records that a FedCM continuation API dialog was shown.
  void RecordFederatedContinuationShown();

  // Records the result of a federated actor login request.
  void RecordFederatedLoginResult(content::webid::FederatedLoginResult result);

  // Records whether a hanging FedCM request exists during an actor login
  // request.
  void RecordFederatedHangingFedCmRequestExists(bool exists);

 private:
  // Metrics recording should only happen during destruction, so this method is
  // private.
  void RecordUkm();

  base::TimeTicks get_credentials_start_time_;
  base::TimeTicks get_credentials_completed_time_;
  base::TimeTicks account_chosen_time_;

  ukm::SourceId source_id_ = ukm::kInvalidSourceId;
  ukm::builders::Actor_Login builder_;
  bool ukm_recorded_ = false;
};

}  // namespace actor_login

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACTOR_LOGIN_INTERNAL_ACTOR_LOGIN_METRICS_HELPER_H_
