// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_TYPE_CONVERTERS_H_

#include "chromeos/components/scanning/mojom/scanning.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace lorgnette {
class ScannerCapabilities;
}  // namespace lorgnette

namespace mojo {

template <>
struct TypeConverter<chromeos::scanning::mojom::ScannerCapabilitiesPtr,
                     lorgnette::ScannerCapabilities> {
  static chromeos::scanning::mojom::ScannerCapabilitiesPtr Convert(
      const lorgnette::ScannerCapabilities& lorgnette_caps);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_CHROMEOS_SCANNING_SCANNING_TYPE_CONVERTERS_H_
