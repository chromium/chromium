// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/shill_log_pii_identifiers.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace system_logs {

const auto kShillPIIMaskedMap =
    base::MakeFixedFlatMap<base::StringPiece, feedback::PIIType>({
        // Masked for devices only in ScrubAndExpandProperties:
        // shill::kAddress,
        {shill::kCellularPPPUsernameProperty, feedback::PIIType::kLocationInfo},
        {shill::kEquipmentIdProperty, feedback::PIIType::kLocationInfo},
        {shill::kEsnProperty, feedback::PIIType::kLocationInfo},
        {shill::kIccidProperty, feedback::PIIType::kLocationInfo},
        {shill::kImeiProperty, feedback::PIIType::kLocationInfo},
        {shill::kImsiProperty, feedback::PIIType::kLocationInfo},
        {shill::kMdnProperty, feedback::PIIType::kLocationInfo},
        {shill::kMeidProperty, feedback::PIIType::kLocationInfo},
        {shill::kMinProperty, feedback::PIIType::kLocationInfo},
        {shill::kUsageURLProperty, feedback::PIIType::kLocationInfo},
        {shill::kEapAnonymousIdentityProperty, feedback::PIIType::kEAP},
        {shill::kEapIdentityProperty, feedback::PIIType::kEAP},
        {shill::kEapPinProperty, feedback::PIIType::kEAP},
        {shill::kEapSubjectAlternativeNameMatchProperty,
         feedback::PIIType::kEAP},
        {shill::kEapSubjectMatchProperty, feedback::PIIType::kEAP},
        // Replaced with logging id for services only in
        // ScrubAndExpandProperties:
        // shill::kName
        {shill::kSSIDProperty, feedback::PIIType::kSSID},
        {shill::kWifiHexSsid, feedback::PIIType::kSSID},
        // UIData extracts properties into sub dictionaries, so look for the
        // base property names.
        {"HexSSID", feedback::PIIType::kSSID},
        {"Identity", feedback::PIIType::kSSID},
    });

}  // namespace system_logs
