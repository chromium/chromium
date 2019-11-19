// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_external_data_policy_observer.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "components/policy/core/common/external_data_fetcher.h"

namespace policy {

void DeviceCloudExternalDataPolicyObserver::Delegate::OnDeviceExternalDataSet(
    const std::string& policy) {}

void DeviceCloudExternalDataPolicyObserver::Delegate::
    OnDeviceExternalDataCleared(const std::string& policy) {}

void DeviceCloudExternalDataPolicyObserver::Delegate::
    OnDeviceExternalDataFetched(const std::string& policy,
                                std::unique_ptr<std::string> data,
                                const base::FilePath& file_path) {}

DeviceCloudExternalDataPolicyObserver::Delegate::~Delegate() {}

DeviceCloudExternalDataPolicyObserver::DeviceCloudExternalDataPolicyObserver(
    PolicyService* policy_service,
    const std::string& policy,
    Delegate* delegate)
    : policy_service_(policy_service), policy_(policy), delegate_(delegate) {
  policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
}

DeviceCloudExternalDataPolicyObserver::
    ~DeviceCloudExternalDataPolicyObserver() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void DeviceCloudExternalDataPolicyObserver::OnPolicyUpdated(
    const PolicyNamespace& ns,
    const PolicyMap& previous,
    const PolicyMap& current) {
  DCHECK(ns == PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));

  const PolicyMap::Entry* previous_entry = previous.Get(policy_);
  const PolicyMap::Entry* current_entry = current.Get(policy_);
  if ((!previous_entry && current_entry) ||
      (previous_entry && !current_entry) ||
      (previous_entry && current_entry &&
       !previous_entry->Equals(*current_entry))) {
    HandleExternalDataPolicyUpdate(current_entry);
  }
}

void DeviceCloudExternalDataPolicyObserver::HandleExternalDataPolicyUpdate(
    const PolicyMap::Entry* entry) {
  if (!entry) {
    delegate_->OnDeviceExternalDataCleared(policy_);
    return;
  }

  delegate_->OnDeviceExternalDataSet(policy_);

  // Invalidate any pending callbacks. They are fetching outdated data.
  weak_factory_.InvalidateWeakPtrs();
  if (entry->external_data_fetcher) {
    entry->external_data_fetcher->Fetch(base::BindOnce(
        &DeviceCloudExternalDataPolicyObserver::OnDeviceExternalDataFetched,
        weak_factory_.GetWeakPtr()));
  } else {
    NOTREACHED();
  }
}

void DeviceCloudExternalDataPolicyObserver::OnDeviceExternalDataFetched(
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  delegate_->OnDeviceExternalDataFetched(policy_, std::move(data), file_path);
}

}  // namespace policy
