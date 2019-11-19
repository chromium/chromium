// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_cloud_policy_manager_builder.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/policy/core/common/cloud/cloud_external_data_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store.h"
#include "content/public/browser/network_service_instance.h"

namespace {

// Directory inside the profile directory where policy-related resources are
// stored.
const base::FilePath::CharType kPolicy[] = FILE_PATH_LITERAL("Policy");

// Directory under kPolicy, in the user's profile dir, where policy for
// components is cached.
const base::FilePath::CharType kComponentsDir[] =
    FILE_PATH_LITERAL("Components");

}  // namespace

namespace policy {

std::unique_ptr<UserCloudPolicyManager> CreateUserCloudPolicyManager(
    const base::FilePath& profile_path,
    SchemaRegistry* schema_registry,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner) {
  std::unique_ptr<UserCloudPolicyStore> store(
      UserCloudPolicyStore::Create(profile_path, background_task_runner));
  if (force_immediate_load)
    store->LoadImmediately();

  const base::FilePath component_policy_cache_dir =
      profile_path.Append(kPolicy).Append(kComponentsDir);

  auto policy_manager = std::make_unique<UserCloudPolicyManager>(
      std::move(store), component_policy_cache_dir,
      std::unique_ptr<CloudExternalDataManager>(),
      base::ThreadTaskRunnerHandle::Get(),
      base::BindRepeating(&content::GetNetworkConnectionTracker));
  policy_manager->Init(schema_registry);
  return policy_manager;
}

}  // namespace policy
