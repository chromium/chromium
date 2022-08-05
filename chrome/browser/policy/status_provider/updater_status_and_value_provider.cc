// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/updater_status_and_value_provider.h"

#include <windows.h>

#include <DSRole.h>
#include <algorithm>
#include <utility>

#include "base/callback_forward.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/google/google_update_policy_fetcher_win.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/install_static/install_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

std::string GetActiveDirectoryDomain() {
  std::string domain;
  ::DSROLE_PRIMARY_DOMAIN_INFO_BASIC* info = nullptr;
  if (::DsRoleGetPrimaryDomainInformation(nullptr,
                                          ::DsRolePrimaryDomainInfoBasic,
                                          (PBYTE*)&info) != ERROR_SUCCESS) {
    return domain;
  }
  if (info->DomainNameDns)
    domain = base::WideToUTF8(info->DomainNameDns);
  ::DsRoleFreeMemory(info);
  return domain;
}

}  // namespace

UpdaterStatusAndValueProvider::UpdaterStatusAndValueProvider(Profile* profile)
    : profile_(profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&GetActiveDirectoryDomain),
      base::BindOnce(&UpdaterStatusAndValueProvider::OnDomainReceived,
                     weak_factory_.GetWeakPtr()));
}

UpdaterStatusAndValueProvider::~UpdaterStatusAndValueProvider() = default;

base::Value::Dict UpdaterStatusAndValueProvider::GetStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict dict;
  if (!domain_.empty())
    dict.Set("domain", domain_);
  if (!updater_status_)
    return dict;
  if (!updater_status_->version.empty())
    dict.Set("version", base::WideToUTF8(updater_status_->version));
  if (!updater_status_->last_checked_time.is_null()) {
    dict.Set("timeSinceLastRefresh",
             GetTimeSinceLastActionString(updater_status_->last_checked_time));
  }
  return dict;
}

void UpdaterStatusAndValueProvider::GetValues(
    base::Value::List& out_policy_values) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!updater_policies_) {
    return;
  }

  base::Value::Dict updater_policies_data;
  updater_policies_data.Set("name", "Google Update Policies");
  updater_policies_data.Set("id", "updater");

  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
  client->EnableConvertValues(true);
  client->SetDropDefaultValues(true);
  // TODO(b/241519819): Find an alternative to using PolicyConversionsClient
  // directly.
  updater_policies_data.Set("policies", client->ConvertUpdaterPolicies(
                                            updater_policies_->Clone(),
                                            GetGoogleUpdatePolicySchemas()));
  out_policy_values.Append(std::move(updater_policies_data));
}

base::Value::Dict UpdaterStatusAndValueProvider::GetNames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Value::Dict names;
  if (updater_policies_) {
    base::Value::Dict updater_policies;
    updater_policies.Set("name", "Google Update Policies");
    updater_policies.Set("policyNames", GetGoogleUpdatePolicyNames());
    names.Set("updater", std::move(updater_policies));
  }
  return names;
}

void UpdaterStatusAndValueProvider::Refresh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskAndReplyWithResult(
      base::ThreadPool::CreateCOMSTATaskRunner(
          {base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN, base::MayBlock()})
          .get(),
      FROM_HERE, base::BindOnce(&GetGoogleUpdatePoliciesAndState),
      base::BindOnce(&UpdaterStatusAndValueProvider::OnUpdaterPoliciesRefreshed,
                     weak_factory_.GetWeakPtr()));
}

void UpdaterStatusAndValueProvider::OnDomainReceived(std::string domain) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  domain_ = std::move(domain);
  // Call Refresh() to load the policies when the domain is received.
  Refresh();
}

void UpdaterStatusAndValueProvider::OnUpdaterPoliciesRefreshed(
    std::unique_ptr<GoogleUpdatePoliciesAndState> updater_policies_and_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  updater_policies_ = std::move(updater_policies_and_state->policies);
  updater_status_ = std::move(updater_policies_and_state->state);
  NotifyValueChange();
  NotifyStatusChange();
}
