// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_FACTORY_H_

#include <memory>

class Profile;

namespace enterprise_connectors {

class SignalsService;

// Returns a SignalsService instance properly configured for the current
// environment.
std::unique_ptr<SignalsService> CreateSignalsService(Profile* profile);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_FACTORY_H_
