// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/status_provider/device_local_account_policy_status_provider.h"

#include "base/values.h"
#include "chrome/browser/policy/status_provider/status_provider_util.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

DeviceLocalAccountPolicyStatusProvider::DeviceLocalAccountPolicyStatusProvider(
    const std::string& user_id,
    policy::DeviceLocalAccountPolicyService* service)
    : user_id_(user_id), service_(service) {
  service_->AddObserver(this);
}

DeviceLocalAccountPolicyStatusProvider::
    ~DeviceLocalAccountPolicyStatusProvider() {
  service_->RemoveObserver(this);
}

base::Value::Dict DeviceLocalAccountPolicyStatusProvider::GetStatus() {
  const policy::DeviceLocalAccountPolicyBroker* broker =
      service_->GetBrokerForUser(user_id_);
  base::Value::Dict dict;
  if (broker) {
    dict = policy::PolicyStatusProvider::GetStatusFromCore(broker->core());
  } else {
    dict.Set("error", true);
    dict.Set("status", policy::FormatStoreStatus(
                           policy::CloudPolicyStore::STATUS_BAD_STATE,
                           policy::CloudPolicyValidatorBase::VALIDATION_OK));
    dict.Set(policy::kUsernameKey, std::string());
  }
  SetDomainExtractedFromUsername(dict);
  dict.Set("publicAccount", true);
  dict.Set(policy::kPolicyDescriptionKey, kUserPolicyStatusDescription);
  return dict;
}

void DeviceLocalAccountPolicyStatusProvider::OnPolicyUpdated(
    const std::string& user_id) {
  if (user_id == user_id_)
    NotifyStatusChange();
}

void DeviceLocalAccountPolicyStatusProvider::OnDeviceLocalAccountsChanged() {
  NotifyStatusChange();
}
