// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_policy_handler.h"

#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"

namespace ash {
namespace platform_keys {

KeyPermissionsPolicyHandler::KeyPermissionsPolicyHandler(
    const policy::Schema& chrome_schema)
    : policy::SchemaValidatingPolicyHandler(
          policy::key::kKeyPermissions,
          chrome_schema.GetKnownProperty(policy::key::kKeyPermissions),
          policy::SCHEMA_ALLOW_UNKNOWN) {}

void KeyPermissionsPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& /* policies */,
    PrefValueMap* /* prefs */) {}

}  // namespace platform_keys
}  // namespace ash
