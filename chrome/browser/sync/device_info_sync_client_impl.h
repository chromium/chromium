// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_CLIENT_IMPL_H_
#define CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_CLIENT_IMPL_H_

#include "base/macros.h"
#include "base/optional.h"

#include "components/sync_device_info/device_info_sync_client.h"

class Profile;

namespace browser_sync {

class DeviceInfoSyncClientImpl : public syncer::DeviceInfoSyncClient {
 public:
  explicit DeviceInfoSyncClientImpl(Profile* profile);

  ~DeviceInfoSyncClientImpl() override;

  // syncer::DeviceInfoSyncClient:
  std::string GetSigninScopedDeviceId() const override;

  // syncer::DeviceInfoSyncClient:
  bool GetSendTabToSelfReceivingEnabled() const override;

  // syncer::DeviceInfoSyncClient:
  base::Optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override;

  // syncer::DeviceInfoSyncClient:
  base::Optional<std::string> GetFCMRegistrationToken() const override;

  // syncer::DeviceInfoSyncClient:
  base::Optional<syncer::ModelTypeSet> GetInterestedDataTypes() const override;

  // syncer::DeviceInfoSyncClient:
  base::Optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
  GetPhoneAsASecurityKeyInfo() const override;

  // syncer::DeviceInfoSyncClient:
  bool IsUmaEnabledOnCrOSDevice() const override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfoSyncClientImpl);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_CLIENT_IMPL_H_
