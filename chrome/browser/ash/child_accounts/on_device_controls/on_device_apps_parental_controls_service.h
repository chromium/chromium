// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_APPS_PARENTAL_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_APPS_PARENTAL_CONTROLS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace ash {

// Service supporting on-device parental controls features for enabling and
// blocking apps.
class OnDeviceAppsParentalControlsService : public KeyedService {
 public:
  OnDeviceAppsParentalControlsService();
  ~OnDeviceAppsParentalControlsService() override;
  OnDeviceAppsParentalControlsService(
      const OnDeviceAppsParentalControlsService&) = delete;
  OnDeviceAppsParentalControlsService& operator=(
      const OnDeviceAppsParentalControlsService&) = delete;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_ON_DEVICE_APPS_PARENTAL_CONTROLS_SERVICE_H_
