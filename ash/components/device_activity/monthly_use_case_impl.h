// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_MONTHLY_USE_CASE_IMPL_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_MONTHLY_USE_CASE_IMPL_H_

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

// Contains the methods required to report the fixed monthly active use case.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) MonthlyUseCaseImpl
    : public DeviceActiveUseCase {
 public:
  MonthlyUseCaseImpl(
      const std::string& psm_device_active_secret,
      const ChromeDeviceMetadataParameters& chrome_passed_device_params,
      PrefService* local_state);
  MonthlyUseCaseImpl(const MonthlyUseCaseImpl&) = delete;
  MonthlyUseCaseImpl& operator=(const MonthlyUseCaseImpl&) = delete;
  ~MonthlyUseCaseImpl() override;

  // Generate the window identifier for the kCrosMonthly use case.
  // For example, the monthly use case should generate a window identifier
  // formatted: yyyyMM.
  //
  // It is generated on demand each time the state machine leaves the idle
  // state. It is reused by several states. It is reset to nullopt. This field
  // is used apart of PSM Import request.
  std::string GenerateUTCWindowIdentifier(base::Time ts) const override;

  // Generate Fresnel PSM import request body.
  // Sets the monthly device metadata dimensions sent by PSM import.
  //
  // Important: Each new dimension added to metadata will need to be approved by
  // privacy.
  ImportDataRequest GenerateImportRequestBody() override;
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_MONTHLY_USE_CASE_IMPL_H_
