// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CRYPTAUTH_GCM_DEVICE_INFO_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_CRYPTAUTH_GCM_DEVICE_INFO_PROVIDER_IMPL_H_

#include "base/no_destructor.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_api.pb.h"
#include "chromeos/ash/services/device_sync/public/cpp/gcm_device_info_provider.h"

namespace ash {

// Concrete GcmDeviceInfoProvider implementation.
class GcmDeviceInfoProviderImpl : public device_sync::GcmDeviceInfoProvider {
 public:
  static const GcmDeviceInfoProviderImpl* GetInstance();

  GcmDeviceInfoProviderImpl(const GcmDeviceInfoProviderImpl&) = delete;
  GcmDeviceInfoProviderImpl& operator=(const GcmDeviceInfoProviderImpl&) =
      delete;

  // device_sync::GcmDeviceInfoProvider:
  const cryptauth::GcmDeviceInfo& GetGcmDeviceInfo() const override;

 private:
  friend class base::NoDestructor<GcmDeviceInfoProviderImpl>;

  GcmDeviceInfoProviderImpl();
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CRYPTAUTH_GCM_DEVICE_INFO_PROVIDER_IMPL_H_
