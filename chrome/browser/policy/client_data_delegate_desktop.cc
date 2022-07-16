// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/client_data_delegate_desktop.h"

#include <utility>

#include "base/callback.h"
#include "base/feature_list.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/features.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

void ClientDataDelegateDesktop::FillRegisterBrowserRequest(
    enterprise_management::RegisterBrowserRequest* request,
    base::OnceClosure callback) const {
  request->set_os_platform(GetOSPlatform());
  request->set_os_version(GetOSVersion());
  request->set_machine_name(GetMachineName());

  if (base::FeatureList::IsEnabled(features::kUploadBrowserDeviceIdentifier)) {
    request->set_allocated_browser_device_identifier(
        GetBrowserDeviceIdentifier().release());
  }

  std::move(callback).Run();
}

}  // namespace policy
