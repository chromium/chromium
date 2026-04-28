// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_TYPE_CONVERTERS_H_

#include <optional>

#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<crosapi::mojom::GetScannerListResponsePtr,
                     lorgnette::ListScannersResponse> {
  static crosapi::mojom::GetScannerListResponsePtr Convert(
      const lorgnette::ListScannersResponse& input);
};

template <>
struct TypeConverter<crosapi::mojom::ScannerOperationResult,
                     lorgnette::OperationResult> {
  static crosapi::mojom::ScannerOperationResult Convert(
      lorgnette::OperationResult input);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_TYPE_CONVERTERS_H_
