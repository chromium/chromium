// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DEPENDENCY_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DEPENDENCY_FACTORY_H_

namespace policy {
class CloudPolicyManager;
}  // namespace policy

namespace enterprise_connectors {

// Factory that can be used to lazy-load dependencies.
class DependencyFactory {
 public:
  virtual ~DependencyFactory() = default;

  virtual policy::CloudPolicyManager* GetUserCloudPolicyManager() const = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DEPENDENCY_FACTORY_H_
