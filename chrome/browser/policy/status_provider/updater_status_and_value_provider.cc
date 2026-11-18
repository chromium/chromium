// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/updater_status_and_value_provider.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/sequence_checker.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/google/google_update_policy_fetcher.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/updater/updater.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"

namespace {

constexpr char kUpdaterPoliciesId[] = "updater";
constexpr char kUpdaterPoliciesName[] = "Google Update Policies";
constexpr char kUpdaterPolicyStatusDescription[] = "statusUpdater";

}  // namespace

UpdaterStatusAndValueProvider::UpdaterStatusAndValueProvider(Profile* profile)
    : profile_(profile) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Init();
}

UpdaterStatusAndValueProvider::~UpdaterStatusAndValueProvider() = default;

base::DictValue UpdaterStatusAndValueProvider::GetStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue dict;
  if (!domain_.empty())
    dict.Set(policy::kDomainKey, domain_);
  if (!updater_status_)
    return dict;
  if (!updater_status_->version.empty())
    dict.Set("version", updater_status_->version);
  if (!updater_status_->last_checked_time.is_null()) {
    dict.Set("timeSinceLastRefresh",
             GetTimeSinceLastActionString(updater_status_->last_checked_time));
  }

  if (dict.empty()) {
    return {};
  }

  dict.Set(policy::kPolicyDescriptionKey, kUpdaterPolicyStatusDescription);
  return dict;
}

policy::mojom::StatusPtr UpdaterStatusAndValueProvider::GetStatusMojo() {
  auto status = policy::mojom::Status::New();
  if (!domain_.empty()) {
    status->domain = domain_;
  }
  if (!updater_status_) {
    return status;
  }
  if (!updater_status_->version.empty()) {
    status->version = updater_status_->version;
  }
  if (!updater_status_->last_checked_time.is_null()) {
    status->time_since_last_refresh = base::UTF16ToUTF8(
        GetTimeSinceLastActionString(updater_status_->last_checked_time));
  }
  status->policy_description_key = kUpdaterPolicyStatusDescription;
  return status;
}

base::DictValue UpdaterStatusAndValueProvider::GetValues() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!updater_policies_)
    return {};

  base::DictValue updater_policies_data;
  updater_policies_data.Set(policy::kNameKey, kUpdaterPoliciesName);

  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
  client->EnableConvertValues(true);
  client->SetDropDefaultValues(true);
  // TODO(b/241519819): Find an alternative to using PolicyConversionsClient
  // directly.
  updater_policies_data.Set(
      policy::kPoliciesKey,
      client->ConvertUpdaterPolicies(updater_policies_->Clone(),
                                     GetGoogleUpdatePolicySchemas()));

  base::DictValue policy_values;
  policy_values.Set(kUpdaterPoliciesId, std::move(updater_policies_data));
  return policy_values;
}

base::DictValue UpdaterStatusAndValueProvider::GetNames() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::DictValue names;
  if (updater_policies_) {
    base::DictValue updater_policies;
    updater_policies.Set(policy::kNameKey, kUpdaterPoliciesName);
    updater_policies.Set(policy::kPolicyNamesKey, GetGoogleUpdatePolicyNames());
    names.Set(kUpdaterPoliciesId, std::move(updater_policies));
  }
  return names;
}

policy::PolicyGroupMojoMap UpdaterStatusAndValueProvider::GetValuesMojo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy::PolicyGroupMojoMap policy_values;
  if (!updater_policies_) {
    return policy_values;
  }

  auto client =
      std::make_unique<policy::ChromePolicyConversionsClient>(profile_);
  // Skip entries marked as default values so chrome://policy only shows updater
  // policies that were actually set.
  client->SetDropDefaultValues(true);

  auto group = policy::mojom::PolicyGroup::New();
  group->name = kUpdaterPoliciesName;
  group->policies = client->ConvertUpdaterPoliciesMojo(
      *updater_policies_, GetGoogleUpdatePolicySchemas());
  policy_values.emplace(kUpdaterPoliciesId, std::move(group));
  return policy_values;
}

policy::PolicyGroupNamesMojoMap UpdaterStatusAndValueProvider::GetNamesMojo()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  policy::PolicyGroupNamesMojoMap names;
  if (!updater_policies_) {
    return names;
  }

  std::vector<std::string> policy_names;
  for (const auto& [policy_name, schema] : GetGoogleUpdatePolicySchemas()) {
    policy_names.push_back(policy_name);
  }
  names.emplace(kUpdaterPoliciesId,
                policy::mojom::PolicyGroupNames::New(kUpdaterPoliciesName,
                                                     std::move(policy_names)));
  return names;
}

void UpdaterStatusAndValueProvider::Refresh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&UpdaterStatusAndValueProvider::DoRefresh,
                                weak_factory_.GetWeakPtr()));
}

void UpdaterStatusAndValueProvider::DoRefresh() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Use a `BarrierCallback` to wait for both async Mojo calls to complete
  // before invoking `OnUpdaterPoliciesRefreshed`. This unified implementation
  // works for both Windows and macOS, ensuring parity with the chrome://updater
  // page.
  auto barrier = base::BarrierCallback<GoogleUpdatePoliciesAndState>(
      2,
      base::BindOnce(&UpdaterStatusAndValueProvider::OnUpdaterPoliciesRefreshed,
                     weak_factory_.GetWeakPtr()));

  // Fetch updater state (version + last_checked).
  updater::GetSystemUpdaterState(
      base::BindOnce([](const updater::mojom::UpdaterState& state) {
        return GoogleUpdatePoliciesAndState{
            .state =
                GoogleUpdateState{
                    .version = state.active_version,
                    .last_checked_time = state.last_checked,
                },
        };
      }).Then(barrier));

  // Fetch policies JSON and parse into PolicyMap.
  updater::GetSystemPoliciesJson(
      base::BindOnce([](const std::string& json) {
        policy::PolicyMap p;
        ParsePoliciesJsonIntoPolicyMap(json, GetUpdaterAppId(), &p);
        return GoogleUpdatePoliciesAndState{.policies = std::move(p)};
      }).Then(barrier));
}

void UpdaterStatusAndValueProvider::OnUpdaterPoliciesRefreshed(
    std::vector<GoogleUpdatePoliciesAndState> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  updater_policies_ = std::nullopt;
  updater_status_ = std::nullopt;

  for (auto& result : results) {
    if (result.state.has_value()) {
      updater_status_ = std::move(*result.state);
    } else if (result.policies.has_value()) {
      updater_policies_ = std::move(*result.policies);
    }
  }
  NotifyValueChange();
  NotifyStatusChange();
}
