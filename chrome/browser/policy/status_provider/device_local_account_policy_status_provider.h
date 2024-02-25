// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_LOCAL_ACCOUNT_POLICY_STATUS_PROVIDER_H_
#define CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_LOCAL_ACCOUNT_POLICY_STATUS_PROVIDER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/core/device_local_account_policy_service.h"
#include "components/policy/core/browser/webui/policy_status_provider.h"

// A cloud policy status provider that reads policy status from the policy core
// associated with the device-local account specified by |user_id| at
// construction time. The indirection via user ID and
// DeviceLocalAccountPolicyService is necessary because the device-local account
// may go away any time behind the scenes, at which point the status message
// text will indicate CloudPolicyStore::STATUS_BAD_STATE.
class DeviceLocalAccountPolicyStatusProvider
    : public policy::PolicyStatusProvider,
      public policy::DeviceLocalAccountPolicyService::Observer {
 public:
  DeviceLocalAccountPolicyStatusProvider(
      const std::string& user_id,
      policy::DeviceLocalAccountPolicyService* service);

  DeviceLocalAccountPolicyStatusProvider(
      const DeviceLocalAccountPolicyStatusProvider&) = delete;
  DeviceLocalAccountPolicyStatusProvider& operator=(
      const DeviceLocalAccountPolicyStatusProvider&) = delete;

  ~DeviceLocalAccountPolicyStatusProvider() override;

  // PolicyStatusProvider implementation.
  base::Value::Dict GetStatus() override;

  // policy::DeviceLocalAccountPolicyService::Observer implementation.
  void OnPolicyUpdated(const std::string& user_id) override;
  void OnDeviceLocalAccountsChanged() override;

 private:
  const std::string user_id_;
  raw_ptr<policy::DeviceLocalAccountPolicyService> service_;
};

#endif  // CHROME_BROWSER_POLICY_STATUS_PROVIDER_DEVICE_LOCAL_ACCOUNT_POLICY_STATUS_PROVIDER_H_
