// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA_MANAGER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base.h"
#include "components/policy/core/common/policy_details.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ResourceCache;

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// This is the implementation for device-local accounts on Chrome OS.
// The manager is created by the DeviceLocalAccountExternalDataService and must
// not outlive it because the |resource_cache| is owned by the service. However,
// the service is not the manager's sole owner: The manager lives as long as
// either the service or a DeviceLocalAccountPolicyProvider references it. This
// is necessary because a device-local account can be removed from policy (thus
// removing it from the service) while a session is in progress and a provider
// exists for the account. The manager is only destroyed when neither service
// nor provider reference it anymore.
class DeviceLocalAccountExternalDataManager
    : public CloudExternalDataManagerBase,
      public base::RefCounted<DeviceLocalAccountExternalDataManager> {
 public:
  DeviceLocalAccountExternalDataManager(
      const DeviceLocalAccountExternalDataManager&) = delete;
  DeviceLocalAccountExternalDataManager& operator=(
      const DeviceLocalAccountExternalDataManager&) = delete;

 private:
  friend class DeviceLocalAccountExternalDataService;
  friend class base::RefCounted<DeviceLocalAccountExternalDataManager>;

  // |get_policy_details| is used to determine the maximum size that the
  // data referenced by each policy can have. Download scheduling, verification,
  // caching and retrieval tasks are done via the |backend_task_runner|, which
  // must support file I/O. The manager is responsible for external data
  // references by policies in |policy_store|. Downloaded external data is
  // stored in the |resource_cache|. The data is keyed by |account_id|, allowing
  // one cache to be shared by any number of accounts. To ensure synchronization
  // of operations on the shared cache, all its users must access the cache via
  // |backend_task_runner| only.
  DeviceLocalAccountExternalDataManager(
      const std::string& account_id,
      const GetChromePolicyDetailsCallback& get_policy_details,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      ResourceCache* resource_cache);
  ~DeviceLocalAccountExternalDataManager() override;

  // CloudExternalDataManagerBase:
  void OnPolicyStoreLoaded() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_DEVICE_LOCAL_ACCOUNT_EXTERNAL_DATA_MANAGER_H_
