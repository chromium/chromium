// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/management_utils.h"

#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "components/policy/core/common/management/management_service.h"

namespace policy {

bool IsDeviceEnterpriseManaged() {
  return policy::ManagementServiceFactory::GetForPlatform()->IsManaged();
}

}  // namespace policy
