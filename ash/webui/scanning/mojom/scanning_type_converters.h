// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SCANNING_MOJOM_SCANNING_TYPE_CONVERTERS_H_
#define ASH_WEBUI_SCANNING_MOJOM_SCANNING_TYPE_CONVERTERS_H_

#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<ash::scanning::mojom::ColorMode, lorgnette::ColorMode> {
  static ash::scanning::mojom::ColorMode ToMojom(
      lorgnette::ColorMode color_mode);
  static bool FromMojom(ash::scanning::mojom::ColorMode input,
                        lorgnette::ColorMode* out);
};

template <>
struct EnumTraits<ash::scanning::mojom::SourceType, lorgnette::SourceType> {
  static ash::scanning::mojom::SourceType ToMojom(
      lorgnette::SourceType source_type);
  static bool FromMojom(ash::scanning::mojom::SourceType input,
                        lorgnette::SourceType* out);
};

template <>
struct EnumTraits<ash::scanning::mojom::FileType, lorgnette::ImageFormat> {
  static ash::scanning::mojom::FileType ToMojom(
      lorgnette::ImageFormat image_format);
  static bool FromMojom(ash::scanning::mojom::FileType input,
                        lorgnette::ImageFormat* out);
};

template <>
struct EnumTraits<ash::scanning::mojom::ScanResult,
                  lorgnette::ScanFailureMode> {
  static ash::scanning::mojom::ScanResult ToMojom(
      lorgnette::ScanFailureMode lorgnette_failure_mode);
  static bool FromMojom(ash::scanning::mojom::ScanResult input,
                        lorgnette::ScanFailureMode* out);
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

#endif  // ASH_WEBUI_SCANNING_MOJOM_SCANNING_TYPE_CONVERTERS_H_
