// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

class Profile;

namespace policy {

// This class gives information related to the browser's management state.
// For more imformation please read
// //components/policy/core/common/management/management_service.md
class BrowserManagementService : public ManagementService, public KeyedService {
 public:
  explicit BrowserManagementService(Profile* profile);
  ~BrowserManagementService() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ENTERPRISE_BROWSER_MANAGEMENT_BROWSER_MANAGEMENT_SERVICE_H_
