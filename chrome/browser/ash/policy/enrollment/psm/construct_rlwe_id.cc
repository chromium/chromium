// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/construct_rlwe_id.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy::psm {

psm_rlwe::RlwePlaintextId ConstructRlweId() {
  // Retrieve the device's serial number and RLZ brand code.
  ash::system::StatisticsProvider* provider =
      ash::system::StatisticsProvider::GetInstance();
  const std::optional<std::string_view> device_serial_number =
      provider->GetMachineID();
  const std::optional<std::string_view> device_rlz_brand_code =
      provider->GetMachineStatistic(ash::system::kRlzBrandCodeKey);

  // Verify the existence of the device's data.
  CHECK(device_serial_number);
  CHECK(!device_serial_number->empty());
  CHECK(device_rlz_brand_code);
  CHECK(!device_rlz_brand_code->empty());

  // Construct the encrypted PSM RLWE ID.
  psm_rlwe::RlwePlaintextId rlwe_id;
  std::string rlz_brand_code_hex = base::HexEncode(
      device_rlz_brand_code->data(), device_rlz_brand_code->size());
  rlwe_id.set_sensitive_id(rlz_brand_code_hex + "/" +
                           std::string(device_serial_number.value()));

  return rlwe_id;
}

}  // namespace policy::psm
