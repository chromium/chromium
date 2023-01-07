// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SECURE_ORIGIN_POLICY_HANDLER_H_
#define CHROME_BROWSER_SSL_SECURE_ORIGIN_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

class SecureOriginPolicyHandler : public SchemaValidatingPolicyHandler {
 public:
  SecureOriginPolicyHandler(const char* policy_name, Schema schema);

  SecureOriginPolicyHandler(const SecureOriginPolicyHandler&) = delete;
  SecureOriginPolicyHandler& operator=(const SecureOriginPolicyHandler&) =
      delete;

  ~SecureOriginPolicyHandler() override;

 protected:
  // ConfigurationPolicyHandler:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_SSL_SECURE_ORIGIN_POLICY_HANDLER_H_
