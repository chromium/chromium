// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_XDR_MANAGER_H_
#define CHROME_BROWSER_ASH_NET_XDR_MANAGER_H_

#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"

namespace ash {

// Responds to changes in the DeviceReportXDREvents policy and updates
// the network metadata to determine if connection warning should be shown.
class XdrManager {
 public:
  explicit XdrManager(policy::PolicyService* policy_service);
  XdrManager(const XdrManager&) = delete;
  XdrManager& operator=(const XdrManager&) = delete;
  ~XdrManager();

  // Returns whether or not XDR events are being reported.
  bool AreXdrPoliciesEnabled();

 private:
  // Updates the network metadata store value when the XDR policy changes.
  void OnXdrPolicyChange(const base::Value* previous,
                         const base::Value* current);
  // Sets the current value of the XDR policy in network metadata store.
  void SetNetworkMetadataStoreXdrValue();

  std::unique_ptr<policy::PolicyChangeRegistrar> policy_registrar_;
  bool report_xdr_events_enabled_ = false;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_XDR_MANAGER_H_
