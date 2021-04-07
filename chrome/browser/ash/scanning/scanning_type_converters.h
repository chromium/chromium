// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCANNING_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_SCANNING_SCANNING_TYPE_CONVERTERS_H_

#include "ash/content/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<ash::scanning::mojom::ScanResult,
                     lorgnette::ScanFailureMode> {
  static ash::scanning::mojom::ScanResult Convert(
      const lorgnette::ScanFailureMode lorgnette_failure_mode);
};

template <>
struct TypeConverter<ash::scanning::mojom::ScannerCapabilitiesPtr,
                     lorgnette::ScannerCapabilities> {
  static ash::scanning::mojom::ScannerCapabilitiesPtr Convert(
      const lorgnette::ScannerCapabilities& lorgnette_caps);
};

template <>
struct TypeConverter<lorgnette::ScanSettings,
                     ash::scanning::mojom::ScanSettingsPtr> {
  static lorgnette::ScanSettings Convert(
      const ash::scanning::mojom::ScanSettingsPtr& mojo_settings);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_ASH_SCANNING_SCANNING_TYPE_CONVERTERS_H_
