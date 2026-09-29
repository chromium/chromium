// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_MEDIA_DEVICE_SALT_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_MEDIA_DEVICE_SALT_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/media_device_salt/media_device_salt_service_provider.h"

namespace ash {

class MediaDeviceSaltServiceProviderImpl
    : public MediaDeviceSaltServiceProvider {
 public:
  MediaDeviceSaltServiceProviderImpl();
  MediaDeviceSaltServiceProviderImpl(
      const MediaDeviceSaltServiceProviderImpl&) = delete;
  MediaDeviceSaltServiceProviderImpl& operator=(
      const MediaDeviceSaltServiceProviderImpl&) = delete;
  ~MediaDeviceSaltServiceProviderImpl() override;

  // MediaDeviceSaltServiceProvider:
  media_device_salt::MediaDeviceSaltService* Find(
      const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_MEDIA_DEVICE_SALT_SERVICE_PROVIDER_IMPL_H_
