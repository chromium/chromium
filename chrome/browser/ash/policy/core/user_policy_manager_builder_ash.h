// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_POLICY_MANAGER_BUILDER_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_POLICY_MANAGER_BUILDER_ASH_H_

#include <memory>

#include "base/memory/ref_counted.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ActiveDirectoryPolicyManager;
class UserCloudPolicyManagerAsh;

// Create a ConfigurationPolicyProvider for the given Profile.
// Either a UserCloudPolicyManagerAsh, an ActiveDirectoryPolicyManager,
// or neither will be returned through the out parameters.
void CreateConfigurationPolicyProvider(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    std::unique_ptr<UserCloudPolicyManagerAsh>*
        user_cloud_policy_manager_ash_out,
    std::unique_ptr<ActiveDirectoryPolicyManager>*
        active_directory_policy_manager_out);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_POLICY_MANAGER_BUILDER_ASH_H_
