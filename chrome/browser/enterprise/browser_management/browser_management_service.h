// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_

#include "base/containers/flat_set.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

class Profile;

namespace policy {

// This class gives information related to the browser's management state.
class BrowserManagementService : public ManagementService {
 public:
  explicit BrowserManagementService(Profile* profile);
  ~BrowserManagementService() override;

 protected:
  // ManagementService impl
  void InitManagementStatusProviders() override;

 private:
  Profile* profile_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
