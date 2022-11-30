// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/user_network_configuration_updater.h"

#include "components/policy/policy_constants.h"

namespace policy {

// static
std::unique_ptr<UserNetworkConfigurationUpdater>
UserNetworkConfigurationUpdater::CreateForUserPolicy(
    PolicyService* policy_service) {
  auto updater =
      std::make_unique<UserNetworkConfigurationUpdater>(policy_service);
  updater->Init();
  return updater;
}

UserNetworkConfigurationUpdater::UserNetworkConfigurationUpdater(
    PolicyService* policy_service)
    : NetworkConfigurationUpdater(onc::ONC_SOURCE_USER_POLICY,
                                  key::kOpenNetworkConfiguration,
                                  policy_service) {}

}  // namespace policy
