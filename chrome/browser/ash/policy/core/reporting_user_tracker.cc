// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/reporting_user_tracker.h"

#include <utility>

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {
namespace {

std::string FullyCanonicalize(const std::string& email) {
  return gaia::CanonicalizeEmail(gaia::SanitizeEmail(email));
}

}  // namespace

ReportingUserTracker::ReportingUserTracker(
    user_manager::UserManager* user_manager)
    : local_state_(user_manager->GetLocalState()) {
  observation_.Observe(user_manager);
}

ReportingUserTracker::~ReportingUserTracker() = default;

// static
void ReportingUserTracker::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(::prefs::kReportingUsers);
}

bool ReportingUserTracker::ShouldReportUser(
    const std::string& user_email) const {
  const base::Value::List& reporting_users =
      local_state_->GetList(::prefs::kReportingUsers);
  base::Value user_email_value(FullyCanonicalize(user_email));
  return base::Contains(reporting_users, user_email_value);
}

void ReportingUserTracker::OnUserAffiliationUpdated(
    const user_manager::User& user) {
  if (!local_state_) {
    // On unittests, |LocalState| may not be initialized.
    // TODO(b/267685577): Consider getting rid of this.
    CHECK_IS_TEST();
    return;
  }

  if (user.GetType() != user_manager::UserType::kRegular) {
    return;
  }

  const AccountId& account_id = user.GetAccountId();
  if (user.IsAffiliated()) {
    AddReportingUser(account_id);
  } else {
    RemoveReportingUser(account_id);
  }
}

void ReportingUserTracker::OnUserRemoved(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  if (!local_state_) {
    // On unittests, |LocalState| may not be initialized.
    // TODO(b/267685577): Consider getting rid of this.
    CHECK_IS_TEST();
    return;
  }

  RemoveReportingUser(account_id);
}

void ReportingUserTracker::AddReportingUser(const AccountId& account_id) {
  ScopedListPrefUpdate users_update(local_state_, ::prefs::kReportingUsers);
  base::Value email_value(FullyCanonicalize(account_id.GetUserEmail()));
  if (!base::Contains(users_update.Get(), email_value)) {
    users_update->Append(std::move(email_value));
  }
}

void ReportingUserTracker::RemoveReportingUser(const AccountId& account_id) {
  ScopedListPrefUpdate users_update(local_state_, ::prefs::kReportingUsers);
  base::Value::List& update_list = users_update.Get();
  auto it = base::ranges::find(
      update_list, base::Value(FullyCanonicalize(account_id.GetUserEmail())));
  if (it == update_list.end()) {
    return;
  }
  update_list.erase(it);
}

}  // namespace policy
