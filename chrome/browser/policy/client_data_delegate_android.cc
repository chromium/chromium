// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/client_data_delegate_android.h"

#include "chrome/browser/policy/android/cloud_management_android_connection.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace policy {

void ClientDataDelegateAndroid::FillRegisterBrowserRequest(
    enterprise_management::RegisterBrowserRequest* request) const {
  request->set_os_platform(GetOSPlatform());
  request->set_os_version(GetOSVersion());
  request->set_device_model(GetDeviceModel());
  request->set_brand_name(GetDeviceManufacturer());

  std::string gservices_android_id = android::GetGservicesAndroidId();
  if (!gservices_android_id.empty()) {
    request->mutable_browser_device_identifier()
        ->mutable_android_identifier()
        ->set_gservices_android_id(gservices_android_id);
  }
}

}  // namespace policy
