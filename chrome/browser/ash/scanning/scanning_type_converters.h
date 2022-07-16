// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SCANNING_SCANNING_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_SCANNING_SCANNING_TYPE_CONVERTERS_H_

#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "chromeos/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::scanning::mojom::ScanResult,
                  lorgnette::ScanFailureMode> {
  static ash::scanning::mojom::ScanResult ToMojom(
      const lorgnette::ScanFailureMode lorgnette_failure_mode);
};

template <>
struct StructTraits<ash::scanning::mojom::ScannerCapabilitiesPtr,
                    lorgnette::ScannerCapabilities> {
  static ash::scanning::mojom::ScannerCapabilitiesPtr ToMojom(
      const lorgnette::ScannerCapabilities& lorgnette_caps);
};

template <>
struct StructTraits<lorgnette::ScanSettings,
                    ash::scanning::mojom::ScanSettingsPtr> {
  static lorgnette::ScanSettings ToMojom(
      const ash::scanning::mojom::ScanSettingsPtr& mojo_settings);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_ASH_SCANNING_SCANNING_TYPE_CONVERTERS_H_
