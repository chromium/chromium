// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/user_cloud_external_data_manager.h"

#include <memory>
#include <optional>

#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "components/policy/core/common/cloud/cloud_external_data_store.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/resource_cache.h"

namespace policy {

namespace {

const char kCacheKey[] = "data";

}  // namespace

UserCloudExternalDataManager::UserCloudExternalDataManager(
    const GetChromePolicyDetailsCallback& get_policy_details,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    const base::FilePath& cache_path,
    CloudPolicyStore* policy_store)
    : CloudExternalDataManagerBase(get_policy_details, backend_task_runner),
      resource_cache_(new ResourceCache(cache_path,
                                        backend_task_runner,
                                        /* max_cache_size */ std::nullopt)) {
  SetPolicyStore(policy_store);
  SetExternalDataStore(std::make_unique<CloudExternalDataStore>(
      kCacheKey, backend_task_runner, resource_cache_));
}

UserCloudExternalDataManager::~UserCloudExternalDataManager() {
  SetExternalDataStore(nullptr);
  backend_task_runner_->DeleteSoon(FROM_HERE, resource_cache_.get());
}

}  // namespace policy
