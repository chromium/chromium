// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_SERVICE_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class PrefRegistrySimple;

namespace ash::on_device_controls {

// Service supporting on-device parental controls features for enabling and
// blocking apps.
class AppControlsService : public KeyedService {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  AppControlsService();
  ~AppControlsService() override;
  AppControlsService(const AppControlsService&) = delete;
  AppControlsService& operator=(const AppControlsService&) = delete;
};

}  // namespace ash::on_device_controls

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_ON_DEVICE_CONTROLS_APP_CONTROLS_SERVICE_H_
