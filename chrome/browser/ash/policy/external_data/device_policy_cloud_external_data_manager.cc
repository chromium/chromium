// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/device_policy_cloud_external_data_manager.h"

#include <stdint.h>
#include <utility>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/resource_cache.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

const char kCacheKey[] = "device_policy_external_data";

// Only used for tests.
int64_t g_cache_max_size_override = 0;

}  // namespace

DevicePolicyCloudExternalDataManager::DevicePolicyCloudExternalDataManager(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    const base::FilePath& cache_path,
    CloudPolicyStore* policy_store)
    : CloudExternalDataManagerBase(get_policy_details, backend_task_runner) {
  int cache_max_size = kDevicePolicyExternalDataResourceCacheSize;
  if (g_cache_max_size_override != 0)
    cache_max_size = g_cache_max_size_override;
  resource_cache_ = std::make_unique<ResourceCache>(
      cache_path, backend_task_runner, cache_max_size);

  SetPolicyStore(policy_store);
  SetExternalDataStore(std::make_unique<CloudExternalDataStore>(
      kCacheKey, backend_task_runner, resource_cache_.get()));
}

DevicePolicyCloudExternalDataManager::~DevicePolicyCloudExternalDataManager() {
  SetExternalDataStore(nullptr);
  ResourceCache* resource_cache_to_delete = resource_cache_.release();
  if (!backend_task_runner_->DeleteSoon(FROM_HERE, resource_cache_to_delete)) {
    // If the task runner is no longer running, it's safe to just delete the
    // object, since no further events will be delivered by external data
    // manager.
    delete resource_cache_to_delete;
  }
}

void DevicePolicyCloudExternalDataManager::SetCacheMaxSizeForTesting(
    int64_t cache_max_size) {
  g_cache_max_size_override = cache_max_size;
}

}  // namespace policy
