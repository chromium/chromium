// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_H_

#include <memory>

#include "base/values.h"
#include "chrome/browser/policy/networking/network_configuration_updater.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/policy_service.h"

namespace policy {

// Handles server and authority certificates from the user ONC policy. Ignores
// client certificates and network configs.
class UserNetworkConfigurationUpdater : public NetworkConfigurationUpdater,
                                        public KeyedService {
 public:
  // Creates an updater that applies the server and authority part of ONC user
  // policy from |policy_service| once the policy service is completely
  // initialized and on each policy change.
  static std::unique_ptr<UserNetworkConfigurationUpdater> CreateForUserPolicy(
      PolicyService* policy_service);

  explicit UserNetworkConfigurationUpdater(PolicyService* policy_service);

  // NetworkConfigurationUpdater
  void ImportClientCertificates() override {}
  void ApplyNetworkPolicy(
      const base::Value::List& network_configs_onc,
      const base::Value::Dict& global_network_config) override {}
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_NETWORKING_USER_NETWORK_CONFIGURATION_UPDATER_H_
