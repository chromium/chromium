// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/cloud_policy_test_utils.h"

#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"

namespace policy {

void GetExpectedDefaultPolicy(PolicyMap* policy_map) {
#if BUILDFLAG(IS_ANDROID)
  policy_map->Set(key::kNTPContentSuggestionsEnabled, POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                  base::Value(false), nullptr);
#elif BUILDFLAG(IS_CHROMEOS)
  SetEnterpriseUsersDefaults(policy_map);
#endif
}

CloudPolicyStore* GetCloudPolicyStore(Profile* profile) {
  if (!profile) {
    return nullptr;
  }
  CloudPolicyManager* manager = profile->GetCloudPolicyManager();
  return manager ? manager->core()->store() : nullptr;
}

}  // namespace policy
