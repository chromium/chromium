// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DAILY_USE_CASE_IMPL_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DAILY_USE_CASE_IMPL_H_

#include "ash/components/device_activity/device_active_use_case.h"
#include "base/component_export.h"
#include "base/time/time.h"

class PrefService;

namespace version_info {
enum class Channel;
}  // namespace version_info

namespace ash {
namespace device_activity {

// Forward declaration from fresnel_service.proto.
class ImportDataRequest;

// Contains the methods required to report the daily active use case.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DailyUseCaseImpl
    : public DeviceActiveUseCase {
 public:
  DailyUseCaseImpl(const std::string& psm_device_active_secret,
                   version_info::Channel chromeos_channel,
                   PrefService* local_state);
  DailyUseCaseImpl(const DailyUseCaseImpl&) = delete;
  DailyUseCaseImpl& operator=(const DailyUseCaseImpl&) = delete;
  ~DailyUseCaseImpl() override;

  // Generate the window identifier for the kCrosDaily use case.
  // For example, the daily use case should generate a window identifier
  // formatted: yyyyMMdd.
  //
  // It is generated on demand each time the state machine leaves the idle
  // state. It is reused by several states. It is reset to nullopt. This field
  // is used apart of PSM Import request.
  std::string GenerateUTCWindowIdentifier(base::Time ts) const override;

  // Generate Fresnel PSM import request body.
  // Sets the daily device metadata dimensions sent by PSM import.
  //
  // Important: Each new dimension added to metadata will need to be approved by
  // privacy.
  ImportDataRequest GenerateImportRequestBody() override;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DAILY_USE_CASE_IMPL_H_
