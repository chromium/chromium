// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_FACTORY_H_

#include <memory>

namespace enterprise_connectors {

class KeyNetworkDelegate;

// A factory function to create a NetworkDelegate.
std::unique_ptr<KeyNetworkDelegate> CreateKeyNetworkDelegate();

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_KEY_NETWORK_DELEGATE_FACTORY_H_
