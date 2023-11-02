// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_DEVICE_POLICY_CLOUD_EXTERNAL_DATA_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_DEVICE_POLICY_CLOUD_EXTERNAL_DATA_MANAGER_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/external_data/cloud_external_data_manager_base.h"
#include "components/policy/core/common/policy_details.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class CloudPolicyStore;
class ResourceCache;

// Downloads, verifies and caches external data referenced by policies.
// This is the implementation for device policy external data.
class DevicePolicyCloudExternalDataManager
    : public CloudExternalDataManagerBase {
 public:
  // |get_policy_details| is used to determine the maximum size that the
  // data referenced by each policy can have. Download scheduling, verification,
  // caching and retrieval tasks are done via the |backend_task_runner|, which
  // must support file I/O. |resource_cache| is used to construct a data store
  // which caches downloaded blobs on disk.
  DevicePolicyCloudExternalDataManager(
      const GetChromePolicyDetailsCallback& get_policy_details,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
      const base::FilePath& cache_path,
      CloudPolicyStore* policy_store);

  DevicePolicyCloudExternalDataManager(
      const DevicePolicyCloudExternalDataManager&) = delete;
  DevicePolicyCloudExternalDataManager& operator=(
      const DevicePolicyCloudExternalDataManager&) = delete;

  ~DevicePolicyCloudExternalDataManager() override;

  // Sets the cache maximum size for testing.
  // It's used to avoid creating big data in tests.
  static void SetCacheMaxSizeForTesting(int64_t cache_max_size);

 private:
  // Cache used by the data store. Must outlive the data store.
  std::unique_ptr<ResourceCache> resource_cache_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_DEVICE_POLICY_CLOUD_EXTERNAL_DATA_MANAGER_H_
