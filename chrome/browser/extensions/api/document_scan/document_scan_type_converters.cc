// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

namespace mojo {

namespace document_scan = extensions::api::document_scan;
namespace mojom = crosapi::mojom;

document_scan::OperationResult
TypeConverter<document_scan::OperationResult, mojom::ScannerOperationResult>::
    Convert(mojom::ScannerOperationResult input) {
  switch (input) {
    case mojom::ScannerOperationResult::kUnknown:
      return document_scan::OperationResult::kUnknown;
    case mojom::ScannerOperationResult::kSuccess:
      return document_scan::OperationResult::kSuccess;
    case mojom::ScannerOperationResult::kUnsupported:
      return document_scan::OperationResult::kUnsupported;
    case mojom::ScannerOperationResult::kCancelled:
      return document_scan::OperationResult::kCancelled;
    case mojom::ScannerOperationResult::kDeviceBusy:
      return document_scan::OperationResult::kDeviceBusy;
    case mojom::ScannerOperationResult::kInvalid:
      return document_scan::OperationResult::kInvalid;
    case mojom::ScannerOperationResult::kWrongType:
      return document_scan::OperationResult::kWrongType;
    case mojom::ScannerOperationResult::kEndOfData:
      return document_scan::OperationResult::kEof;
    case mojom::ScannerOperationResult::kAdfJammed:
      return document_scan::OperationResult::kAdfJammed;
    case mojom::ScannerOperationResult::kAdfEmpty:
      return document_scan::OperationResult::kAdfEmpty;
    case mojom::ScannerOperationResult::kCoverOpen:
      return document_scan::OperationResult::kCoverOpen;
    case mojom::ScannerOperationResult::kIoError:
      return document_scan::OperationResult::kIoError;
    case mojom::ScannerOperationResult::kAccessDenied:
      return document_scan::OperationResult::kAccessDenied;
    case mojom::ScannerOperationResult::kNoMemory:
      return document_scan::OperationResult::kNoMemory;
    case mojom::ScannerOperationResult::kDeviceUnreachable:
      return document_scan::OperationResult::kUnreachable;
    case mojom::ScannerOperationResult::kDeviceMissing:
      return document_scan::OperationResult::kMissing;
    case mojom::ScannerOperationResult::kInternalError:
      return document_scan::OperationResult::kInternalError;
  }
}

template <>
struct TypeConverter<document_scan::ConnectionType,
                     mojom::ScannerInfo_ConnectionType> {
  static document_scan::ConnectionType Convert(
      mojom::ScannerInfo_ConnectionType input) {
    switch (input) {
      case mojom::ScannerInfo_ConnectionType::kUnspecified:
        return document_scan::ConnectionType::kUnspecified;
      case mojom::ScannerInfo_ConnectionType::kUsb:
        return document_scan::ConnectionType::kUsb;
      case mojom::ScannerInfo_ConnectionType::kNetwork:
        return document_scan::ConnectionType::kNetwork;
    }
  }
};

crosapi::mojom::ScannerEnumFilterPtr
TypeConverter<crosapi::mojom::ScannerEnumFilterPtr,
              extensions::api::document_scan::DeviceFilter>::
    Convert(const extensions::api::document_scan::DeviceFilter& input) {
  auto output = crosapi::mojom::ScannerEnumFilter::New();
  output->local = input.local.value_or(false);
  output->secure = input.secure.value_or(false);
  return output;
}

extensions::api::document_scan::GetScannerListResponse
TypeConverter<extensions::api::document_scan::GetScannerListResponse,
              crosapi::mojom::GetScannerListResponsePtr>::
    Convert(const crosapi::mojom::GetScannerListResponsePtr& input) {
  document_scan::GetScannerListResponse output;
  output.result = ConvertTo<document_scan::OperationResult>(input->result);
  for (const mojom::ScannerInfoPtr& scanner_in : input->scanners) {
    document_scan::ScannerInfo& scanner_out = output.scanners.emplace_back();
    scanner_out.scanner_id = scanner_in->id;
    scanner_out.name = scanner_in->display_name;
    scanner_out.manufacturer = scanner_in->manufacturer;
    scanner_out.model = scanner_in->model;
    scanner_out.device_uuid = scanner_in->device_uuid;
    scanner_out.connection_type =
        ConvertTo<document_scan::ConnectionType>(scanner_in->connection_type);
    scanner_out.secure = scanner_in->secure;
    scanner_out.image_formats = scanner_in->image_formats;
  }
  return output;
}

}  // namespace mojo
