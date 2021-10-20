// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
#define ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_

#include "base/component_export.h"

namespace ash {
// FirmwareUpdateManager contains all logic that runs the firmware update SWA.
class COMPONENT_EXPORT(ASH_FIRMWARE_UPDATE_MANAGER) FirmwareUpdateManager {
 public:
  FirmwareUpdateManager();
  FirmwareUpdateManager(const FirmwareUpdateManager&) = delete;
  FirmwareUpdateManager& operator=(const FirmwareUpdateManager&) = delete;
  ~FirmwareUpdateManager();

  // Gets the global instance pointer.
  static FirmwareUpdateManager* Get();
};
}  // namespace ash

#endif  // ASH_COMPONENTS_FWUPD_FIRMWARE_UPDATE_MANAGER_H_
