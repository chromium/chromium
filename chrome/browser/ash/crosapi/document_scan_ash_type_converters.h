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

template <>
struct TypeConverter<crosapi::mojom::OpenScannerResponsePtr,
                     lorgnette::OpenScannerResponse> {
  static crosapi::mojom::OpenScannerResponsePtr Convert(
      const lorgnette::OpenScannerResponse& input);
};

template <>
struct TypeConverter<crosapi::mojom::CloseScannerResponsePtr,
                     lorgnette::CloseScannerResponse> {
  static crosapi::mojom::CloseScannerResponsePtr Convert(
      const lorgnette::CloseScannerResponse& input);
};

template <>
struct TypeConverter<crosapi::mojom::StartPreparedScanResponsePtr,
                     lorgnette::StartPreparedScanResponse> {
  static crosapi::mojom::StartPreparedScanResponsePtr Convert(
      const lorgnette::StartPreparedScanResponse& input);
};

template <>
struct TypeConverter<crosapi::mojom::ReadScanDataResponsePtr,
                     lorgnette::ReadScanDataResponse> {
  static crosapi::mojom::ReadScanDataResponsePtr Convert(
      const lorgnette::ReadScanDataResponse& input);
};

template <>
struct TypeConverter<std::optional<lorgnette::ScannerOption>,
                     crosapi::mojom::OptionSettingPtr> {
  static std::optional<lorgnette::ScannerOption> Convert(
      const crosapi::mojom::OptionSettingPtr& input);
};

template <>
struct TypeConverter<crosapi::mojom::SetOptionsResponsePtr,
                     lorgnette::SetOptionsResponse> {
  static crosapi::mojom::SetOptionsResponsePtr Convert(
      const lorgnette::SetOptionsResponse& input);
};

template <>
struct TypeConverter<crosapi::mojom::GetOptionGroupsResponsePtr,
                     lorgnette::GetCurrentConfigResponse> {
  static crosapi::mojom::GetOptionGroupsResponsePtr Convert(
      const lorgnette::GetCurrentConfigResponse& input);
};

template <>
struct TypeConverter<crosapi::mojom::CancelScanResponsePtr,
                     lorgnette::CancelScanResponse> {
  static crosapi::mojom::CancelScanResponsePtr Convert(
      const lorgnette::CancelScanResponse& input);
};

// Types that don't need to be converted directly, but are easier to test in
// isolation.
crosapi::mojom::OptionType ConvertForTesting(lorgnette::OptionType input);
crosapi::mojom::OptionUnit ConvertForTesting(lorgnette::OptionUnit input);
crosapi::mojom::OptionConstraintType ConvertForTesting(
    lorgnette::OptionConstraint_ConstraintType input);
crosapi::mojom::OptionConstraintPtr ConvertForTesting(
    const lorgnette::OptionConstraint& input);
crosapi::mojom::ScannerOptionPtr ConvertForTesting(
    const lorgnette::ScannerOption& input);

}  // namespace mojo

#endif  // CHROME_BROWSER_ASH_CROSAPI_DOCUMENT_SCAN_ASH_TYPE_CONVERTERS_H_
