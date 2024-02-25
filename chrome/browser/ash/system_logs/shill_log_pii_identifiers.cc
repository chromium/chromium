// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/shill_log_pii_identifiers.h"

#include <string_view>

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace system_logs {

const auto kShillPIIMaskedMap =
    base::MakeFixedFlatMap<std::string_view, redaction::PIIType>({
        // Masked for devices only in ScrubAndExpandProperties:
        // shill::kAddress,
        {shill::kCellularPPPUsernameProperty,
         redaction::PIIType::kCellularLocationInfo},
        {shill::kEquipmentIdProperty,
         redaction::PIIType::kCellularLocationInfo},
        {shill::kEsnProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kIccidProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kImeiProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kImsiProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kMdnProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kMeidProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kMinProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kUsageURLProperty, redaction::PIIType::kCellularLocationInfo},
        {shill::kEapAnonymousIdentityProperty, redaction::PIIType::kEAP},
        {shill::kEapIdentityProperty, redaction::PIIType::kEAP},
        {shill::kEapPinProperty, redaction::PIIType::kEAP},
        {shill::kEapSubjectAlternativeNameMatchProperty,
         redaction::PIIType::kEAP},
        {shill::kEapSubjectMatchProperty, redaction::PIIType::kEAP},
        // Replaced with logging id for services only in
        // ScrubAndExpandProperties:
        // shill::kName
        {shill::kSSIDProperty, redaction::PIIType::kSSID},
        {shill::kWifiHexSsid, redaction::PIIType::kSSID},
        // UIData extracts properties into sub dictionaries, so look for the
        // base property names.
        {"HexSSID", redaction::PIIType::kSSID},
        {"Identity", redaction::PIIType::kSSID},
    });

}  // namespace system_logs
