// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DEPENDENCY_FACTORY_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DEPENDENCY_FACTORY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/signals/dependency_factory.h"

class Profile;

namespace enterprise_connectors {

class DependencyFactoryImpl : public DependencyFactory {
 public:
  explicit DependencyFactoryImpl(Profile* profile);

  ~DependencyFactoryImpl() override;

  // DependencyFactory:
  policy::CloudPolicyManager* GetUserCloudPolicyManager() const override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_DEPENDENCY_FACTORY_IMPL_H_
