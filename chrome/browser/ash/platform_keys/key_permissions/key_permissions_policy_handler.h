// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class Schema;
}

namespace ash {
namespace platform_keys {

class KeyPermissionsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit KeyPermissionsPolicyHandler(const policy::Schema& chrome_schema);

  KeyPermissionsPolicyHandler(const KeyPermissionsPolicyHandler&) = delete;
  KeyPermissionsPolicyHandler& operator=(const KeyPermissionsPolicyHandler&) =
      delete;

  // policy::ConfigurationPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace platform_keys
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_POLICY_HANDLER_H_
