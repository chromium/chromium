// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_FACTORY_H_

#include <memory>

class PolicyBlocklistService;
class Profile;

namespace policy {
class ManagementService;
}

namespace enterprise_connectors {

class SignalsService;

// Returns a SignalsService instance properly configured for the current
// environment.
std::unique_ptr<SignalsService> CreateSignalsService(
    Profile* profile,
    PolicyBlocklistService* policy_blocklist_service,
    policy::ManagementService* management_service);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_FACTORY_H_
