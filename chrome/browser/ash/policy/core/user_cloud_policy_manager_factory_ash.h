// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_MANAGER_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_MANAGER_FACTORY_ASH_H_

#include <memory>

#include "base/memory/ref_counted.h"

class Profile;

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class UserCloudPolicyManagerAsh;

// Create a UserCloudPolicyManagerAsh for the given Profile.
// Will return `nullptr` if
//   - `profile` is not a user profile
//   - the user corresponding to `profile` is not an enterprise or child user
//   - the user has no Gaia account
//   - `force_immediate_load` and policy check is still required
std::unique_ptr<UserCloudPolicyManagerAsh> CreateUserCloudPolicyManagerAsh(
    Profile* profile,
    bool force_immediate_load,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_USER_CLOUD_POLICY_MANAGER_FACTORY_ASH_H_
