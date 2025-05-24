// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_SIGNALS_PROFILE_SIGNALS_COLLECTOR_H_
#define CHROME_BROWSER_ENTERPRISE_SIGNALS_PROFILE_SIGNALS_COLLECTOR_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

class PrefService;
class Profile;
class PolicyBlocklistService;

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_connectors {
class ConnectorsService;
}  // namespace enterprise_connectors

namespace device_signals {

class ProfileSignalsCollector : public BaseSignalsCollector {
 public:
  explicit ProfileSignalsCollector(Profile* profile);

  ProfileSignalsCollector(const ProfileSignalsCollector&) = delete;
  ProfileSignalsCollector& operator=(const ProfileSignalsCollector&) = delete;

  ~ProfileSignalsCollector() override;

 private:
  void GetProfileSignals(UserPermission permission,
                         const SignalsAggregationRequest& request,
                         SignalsAggregationResponse& response,
                         base::OnceClosure done_closure);

  const raw_ptr<PolicyBlocklistService> policy_blocklist_service_;
  const raw_ptr<PrefService> profile_prefs_;
  const raw_ptr<policy::CloudPolicyManager> policy_manager_;
  const raw_ptr<enterprise_connectors::ConnectorsService> connectors_service_;
  base::WeakPtrFactory<ProfileSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // CHROME_BROWSER_ENTERPRISE_SIGNALS_PROFILE_SIGNALS_COLLECTOR_H_
