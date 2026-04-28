// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"

#include <cstdint>

#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace mojo {

template <>
struct TypeConverter<crosapi::mojom::ScannerInfo_ConnectionType,
                     lorgnette::ConnectionType> {
  static crosapi::mojom::ScannerInfo_ConnectionType Convert(
      lorgnette::ConnectionType input) {
    using ConnectionType = crosapi::mojom::ScannerInfo_ConnectionType;

    switch (input) {
      default:
        NOTREACHED();
      case lorgnette::ConnectionType::CONNECTION_UNSPECIFIED:
        return ConnectionType::kUnspecified;
      case lorgnette::ConnectionType::CONNECTION_USB:
        return ConnectionType::kUsb;
      case lorgnette::ConnectionType::CONNECTION_NETWORK:
        return ConnectionType::kNetwork;
    }
  }
};

template <>
struct TypeConverter<crosapi::mojom::ScannerInfoPtr, lorgnette::ScannerInfo> {
  static crosapi::mojom::ScannerInfoPtr Convert(
      const lorgnette::ScannerInfo& input) {
    auto output = crosapi::mojom::ScannerInfo::New();
    output->id = input.name();
    output->display_name = input.display_name();
    output->manufacturer = input.manufacturer();
    output->model = input.model();
    output->device_uuid = input.device_uuid();
    output->connection_type =
        ConvertTo<crosapi::mojom::ScannerInfo_ConnectionType>(
            input.connection_type());
    output->secure = input.secure();
    output->image_formats.reserve(input.image_format_size());
    for (const std::string& format : input.image_format()) {
      output->image_formats.emplace_back(format);
    }
    output->protocol_type = input.protocol_type();
    return output;
  }
};

crosapi::mojom::ScannerOperationResult TypeConverter<
    crosapi::mojom::ScannerOperationResult,
    lorgnette::OperationResult>::Convert(lorgnette::OperationResult input) {
  switch (input) {
    default:
      NOTREACHED();
    case lorgnette::OPERATION_RESULT_UNKNOWN:
      return crosapi::mojom::ScannerOperationResult::kUnknown;
    case lorgnette::OPERATION_RESULT_SUCCESS:
      return crosapi::mojom::ScannerOperationResult::kSuccess;
    case lorgnette::OPERATION_RESULT_UNSUPPORTED:
      return crosapi::mojom::ScannerOperationResult::kUnsupported;
    case lorgnette::OPERATION_RESULT_CANCELLED:
      return crosapi::mojom::ScannerOperationResult::kCancelled;
    case lorgnette::OPERATION_RESULT_DEVICE_BUSY:
      return crosapi::mojom::ScannerOperationResult::kDeviceBusy;
    case lorgnette::OPERATION_RESULT_INVALID:
      return crosapi::mojom::ScannerOperationResult::kInvalid;
    case lorgnette::OPERATION_RESULT_WRONG_TYPE:
      return crosapi::mojom::ScannerOperationResult::kWrongType;
    case lorgnette::OPERATION_RESULT_EOF:
      return crosapi::mojom::ScannerOperationResult::kEndOfData;
    case lorgnette::OPERATION_RESULT_ADF_JAMMED:
      return crosapi::mojom::ScannerOperationResult::kAdfJammed;
    case lorgnette::OPERATION_RESULT_ADF_EMPTY:
      return crosapi::mojom::ScannerOperationResult::kAdfEmpty;
    case lorgnette::OPERATION_RESULT_COVER_OPEN:
      return crosapi::mojom::ScannerOperationResult::kCoverOpen;
    case lorgnette::OPERATION_RESULT_IO_ERROR:
      return crosapi::mojom::ScannerOperationResult::kIoError;
    case lorgnette::OPERATION_RESULT_ACCESS_DENIED:
      return crosapi::mojom::ScannerOperationResult::kAccessDenied;
    case lorgnette::OPERATION_RESULT_NO_MEMORY:
      return crosapi::mojom::ScannerOperationResult::kNoMemory;
    case lorgnette::OPERATION_RESULT_UNREACHABLE:
      return crosapi::mojom::ScannerOperationResult::kDeviceUnreachable;
    case lorgnette::OPERATION_RESULT_MISSING:
      return crosapi::mojom::ScannerOperationResult::kDeviceMissing;
    case lorgnette::OPERATION_RESULT_INTERNAL_ERROR:
      return crosapi::mojom::ScannerOperationResult::kInternalError;
  }
}

crosapi::mojom::GetScannerListResponsePtr
TypeConverter<crosapi::mojom::GetScannerListResponsePtr,
              lorgnette::ListScannersResponse>::
    Convert(const lorgnette::ListScannersResponse& input) {
  auto output = crosapi::mojom::GetScannerListResponse::New();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());
  output->scanners.reserve(input.scanners().size());
  for (const auto& scanner : input.scanners()) {
    output->scanners.emplace_back(crosapi::mojom::ScannerInfo::From(scanner));
  }
  return output;
}

}  // namespace mojo
