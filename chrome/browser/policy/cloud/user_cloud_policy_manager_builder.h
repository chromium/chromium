// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_BUILDER_H_
#define CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_BUILDER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace policy {
class SchemaRegistry;
class UserCloudPolicyManager;

std::unique_ptr<UserCloudPolicyManager> CreateUserCloudPolicyManager(
    const base::FilePath& profile_path,
    SchemaRegistry* schema_registry,
    bool force_immediate_load,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CLOUD_USER_CLOUD_POLICY_MANAGER_BUILDER_H_
