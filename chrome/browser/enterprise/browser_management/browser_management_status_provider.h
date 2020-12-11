// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_STATUS_PROVIDER_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_STATUS_PROVIDER_H_

#include "base/containers/flat_set.h"
#include "components/policy/core/common/management/management_service.h"

using EnterpriseManagementAuthority = policy::EnterpriseManagementAuthority;
using ManagementAuthorityTrustworthiness =
    policy::ManagementAuthorityTrustworthiness;

class Profile;

class BrowserCloudManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  BrowserCloudManagementStatusProvider();
  ~BrowserCloudManagementStatusProvider() final;
  bool IsManaged() final;
  EnterpriseManagementAuthority GetAuthority() final;
};

class LocalBrowserManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  LocalBrowserManagementStatusProvider();
  ~LocalBrowserManagementStatusProvider() final;
  bool IsManaged() final;
  EnterpriseManagementAuthority GetAuthority() final;
};

class ProfileCloudManagementStatusProvider final
    : public policy::ManagementStatusProvider {
 public:
  explicit ProfileCloudManagementStatusProvider(Profile* profile);
  ~ProfileCloudManagementStatusProvider() final;
  bool IsManaged() final;
  EnterpriseManagementAuthority GetAuthority() final;

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_STATUS_PROVIDER_H_
