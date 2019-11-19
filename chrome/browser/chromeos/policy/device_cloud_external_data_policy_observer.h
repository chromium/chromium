// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

// Helper for implementing policies referencing external data: This class
// observes a given |policy_| and fetches the external data that it references
// for the device. Notifications are emitted when an external data reference is
// set, cleared or an external data fetch completes successfully.
//
// State is kept at runtime only: External data references that already exist
// when the class is instantiated are considered new, causing a notification to
// be emitted that an external data reference has been set and the referenced
// external data to be fetched.
class DeviceCloudExternalDataPolicyObserver : public PolicyService::Observer {
 public:
  class Delegate {
   public:
    // Invoked when an device external data reference is set.
    virtual void OnDeviceExternalDataSet(const std::string& policy);

    // Invoked when the device external data reference is cleared.
    virtual void OnDeviceExternalDataCleared(const std::string& policy);

    // Invoked when the device external data has been fetched.
    // Failed fetches are retried and the method is called only when a fetch
    // eventually succeeds. If a fetch fails permanently (e.g. because the
    // external data reference specifies an invalid URL), the method is not
    // called at all.
    virtual void OnDeviceExternalDataFetched(const std::string& policy,
                                             std::unique_ptr<std::string> data,
                                             const base::FilePath& file_path);

   protected:
    virtual ~Delegate();
  };

  // |policy_service| should be the device policy service.
  DeviceCloudExternalDataPolicyObserver(PolicyService* policy_service,
                                        const std::string& policy,
                                        Delegate* delegate);
  ~DeviceCloudExternalDataPolicyObserver() override;

  // PolicyService::Observer:
  void OnPolicyUpdated(const PolicyNamespace& ns,
                       const PolicyMap& previous,
                       const PolicyMap& current) override;

 private:
  // Handles the new policy map |entry| by canceling any external data fetch
  // currently in progress, emitting a notification that an external data
  // reference has been cleared (if |entry| is NULL) or set (otherwise),
  // starting a new external data fetch in the latter case.
  void HandleExternalDataPolicyUpdate(const PolicyMap::Entry* entry);

  void OnDeviceExternalDataFetched(std::unique_ptr<std::string> data,
                                   const base::FilePath& file_path);

  PolicyService* const policy_service_;

  // The policy that |this| observes.
  const std::string policy_;

  // Delegate that takes care of policy data updates. Cannot be null.
  Delegate* const delegate_;

  base::WeakPtrFactory<DeviceCloudExternalDataPolicyObserver> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(DeviceCloudExternalDataPolicyObserver);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DEVICE_CLOUD_EXTERNAL_DATA_POLICY_OBSERVER_H_
