// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TYPE_CONVERTERS_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TYPE_CONVERTERS_H_

#include <optional>

#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "mojo/public/cpp/bindings/type_converter.h"

namespace lorgnette {
class CancelScanResponse;
class CloseScannerResponse;
class GetCurrentConfigResponse;
class OpenScannerResponse;
class ReadScanDataResponse;
class ScannerOption;
class SetOptionsResponse;
class StartPreparedScanResponse;
enum OperationResult : int;
}  // namespace lorgnette

namespace extensions::api::document_scan {

OperationResult ConvertLorgnetteOperationResult(
    lorgnette::OperationResult result);

OpenScannerResponse ConvertLorgnetteOpenScannerResponse(
    const lorgnette::OpenScannerResponse& response);

CancelScanResponse ConvertLorgnetteCancelScanResponse(
    const lorgnette::CancelScanResponse& response);

CloseScannerResponse ConvertLorgnetteCloseScannerResponse(
    const lorgnette::CloseScannerResponse& response);

GetOptionGroupsResponse ConvertLorgnetteGetCurrentConfigResponse(
    const lorgnette::GetCurrentConfigResponse& response);

StartScanResponse ConvertLorgnetteStartPreparedScanResponse(
    const lorgnette::StartPreparedScanResponse& response);

ReadScanDataResponse ConvertLorgnetteReadScanDataResponse(
    const lorgnette::ReadScanDataResponse& response);

// Adapts and converts a Lorgnette SetOptionsResponse.
// The results for invalid option names are overridden to be kWrongType.
SetOptionsResponse TransformLorgnetteSetOptionsResponse(
    const lorgnette::SetOptionsResponse& response,
    const std::vector<std::string>& invalid_option_names);

// Adapts and converts an OptionSetting to a Lorgnette ScannerOption.
//
// Even if the caller passed syntactically valid numeric values in
// Javascript, the result that arrives here in the extension implementation can
// contain inconsistencies in double vs integer. These can happen due to the
// inherent JS use of double for integers as well as quirks of how the
// auto-generated IDL mapping code decides to parse arrays for types that accept
// multiple list types. We detect these specific cases and move the value into
// the expected fixed or int field. All other types are assumed to be supplied
// correctly by the caller if they have made it through the JS bindings.
std::optional<lorgnette::ScannerOption>
TransformOptionSettingToLorgnetteScannerOption(const OptionSetting& setting);

OptionType ConvertLorgnetteOptionTypeForTesting(
    const lorgnette::OptionType& input);
ConstraintType ConvertLorgnetteOptionConstraintTypeForTesting(
    const lorgnette::OptionConstraint_ConstraintType& input);
OptionUnit ConvertLorgnetteOptionUnitForTesting(
    const lorgnette::OptionUnit& input);
OptionConstraint ConvertLorgnetteOptionConstraintForTesting(
    const lorgnette::OptionConstraint& input);
std::optional<ScannerOption::Value> GetLorgnetteOptionValueForTesting(
    const lorgnette::ScannerOption& option);
ScannerOption ConvertLorgnetteScannerOptionForTesting(
    const lorgnette::ScannerOption& input);

}  // namespace extensions::api::document_scan

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
struct TypeConverter<extensions::api::document_scan::GetOptionGroupsResponse,
                     crosapi::mojom::GetOptionGroupsResponsePtr> {
  static extensions::api::document_scan::GetOptionGroupsResponse Convert(
      const crosapi::mojom::GetOptionGroupsResponsePtr& input);
};

}  // namespace mojo

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_TYPE_CONVERTERS_H_
