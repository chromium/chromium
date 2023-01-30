// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/shill_log_pii_identifiers.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace system_logs {

const auto kShillPIIMaskedMap =
    base::MakeFixedFlatMap<base::StringPiece, redaction::PIIType>({
        // Masked for devices only in ScrubAndExpandProperties:
        // shill::kAddress,
        {shill::kCellularPPPUsernameProperty, redaction::PIIType::kLocationInfo},
        {shill::kEquipmentIdProperty, redaction::PIIType::kLocationInfo},
        {shill::kEsnProperty, redaction::PIIType::kLocationInfo},
        {shill::kIccidProperty, redaction::PIIType::kLocationInfo},
        {shill::kImeiProperty, redaction::PIIType::kLocationInfo},
        {shill::kImsiProperty, redaction::PIIType::kLocationInfo},
        {shill::kMdnProperty, redaction::PIIType::kLocationInfo},
        {shill::kMeidProperty, redaction::PIIType::kLocationInfo},
        {shill::kMinProperty, redaction::PIIType::kLocationInfo},
        {shill::kUsageURLProperty, redaction::PIIType::kLocationInfo},
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
