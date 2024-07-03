// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CORE_DEPENDENCY_FACTORY_IMPL_H_
#define CHROME_BROWSER_ENTERPRISE_CORE_DEPENDENCY_FACTORY_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "components/enterprise/core/dependency_factory.h"

class Profile;

namespace enterprise_core {

class DependencyFactoryImpl : public DependencyFactory {
 public:
  explicit DependencyFactoryImpl(Profile* profile);

  ~DependencyFactoryImpl() override;

  // DependencyFactory:
  policy::CloudPolicyManager* GetUserCloudPolicyManager() const override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_core

#endif  // CHROME_BROWSER_ENTERPRISE_CORE_DEPENDENCY_FACTORY_IMPL_H_
