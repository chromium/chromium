// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TYPE_CONVERTERS_H_

#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace mojo {

template <>
struct TypeConverter<extensions::api::document_scan::OperationResult,
                     crosapi::mojom::ScannerOperationResult> {
  static extensions::api::document_scan::OperationResult Convert(
      crosapi::mojom::ScannerOperationResult input);
};

template <>
struct TypeConverter<crosapi::mojom::ScannerEnumFilterPtr,
                     extensions::api::document_scan::DeviceFilter> {
  static crosapi::mojom::ScannerEnumFilterPtr Convert(
      const extensions::api::document_scan::DeviceFilter& input);
};

template <>
struct TypeConverter<extensions::api::document_scan::GetScannerListResponse,
                     crosapi::mojom::GetScannerListResponsePtr> {
  static extensions::api::document_scan::GetScannerListResponse Convert(
      const crosapi::mojom::GetScannerListResponsePtr& input);
};

template <>
struct TypeConverter<extensions::api::document_scan::OpenScannerResponse,
                     crosapi::mojom::OpenScannerResponsePtr> {
  static extensions::api::document_scan::OpenScannerResponse Convert(
      const crosapi::mojom::OpenScannerResponsePtr& input);
};

template <>
struct TypeConverter<extensions::api::document_scan::CloseScannerResponse,
                     crosapi::mojom::CloseScannerResponsePtr> {
  static extensions::api::document_scan::CloseScannerResponse Convert(
      const crosapi::mojom::CloseScannerResponsePtr& input);
};

// Test wrappers for type conversions that don't need to be done explicitly.
// This lets them be tested in isolation without fully exposing the
// TypeConverter instances.
extensions::api::document_scan::OptionType ConvertForTesting(
    crosapi::mojom::OptionType input);
extensions::api::document_scan::OptionUnit ConvertForTesting(
    crosapi::mojom::OptionUnit input);
extensions::api::document_scan::ConstraintType ConvertForTesting(
    crosapi::mojom::OptionConstraintType input);
extensions::api::document_scan::Configurability ConvertForTesting(
    crosapi::mojom::OptionConfigurability input);
extensions::api::document_scan::OptionConstraint ConvertForTesting(
    const crosapi::mojom::OptionConstraintPtr& input);
extensions::api::document_scan::ScannerOption::Value ConvertForTesting(
    const crosapi::mojom::OptionValuePtr& input);
extensions::api::document_scan::ScannerOption ConvertForTesting(
    const crosapi::mojom::ScannerOptionPtr& input);

}  // namespace mojo

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TYPE_CONVERTERS_H_
