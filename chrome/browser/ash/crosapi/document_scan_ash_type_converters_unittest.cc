// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(DocumentScanAshTypeConvertersTest, OperationResult) {
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_UNKNOWN),
            crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_SUCCESS),
            crosapi::mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_UNSUPPORTED),
            crosapi::mojom::ScannerOperationResult::kUnsupported);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_CANCELLED),
            crosapi::mojom::ScannerOperationResult::kCancelled);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_DEVICE_BUSY),
            crosapi::mojom::ScannerOperationResult::kDeviceBusy);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_INVALID),
            crosapi::mojom::ScannerOperationResult::kInvalid);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_WRONG_TYPE),
            crosapi::mojom::ScannerOperationResult::kWrongType);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_EOF),
            crosapi::mojom::ScannerOperationResult::kEndOfData);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_ADF_JAMMED),
            crosapi::mojom::ScannerOperationResult::kAdfJammed);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_ADF_EMPTY),
            crosapi::mojom::ScannerOperationResult::kAdfEmpty);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_COVER_OPEN),
            crosapi::mojom::ScannerOperationResult::kCoverOpen);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_IO_ERROR),
            crosapi::mojom::ScannerOperationResult::kIoError);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_ACCESS_DENIED),
            crosapi::mojom::ScannerOperationResult::kAccessDenied);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_NO_MEMORY),
            crosapi::mojom::ScannerOperationResult::kNoMemory);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_UNREACHABLE),
            crosapi::mojom::ScannerOperationResult::kDeviceUnreachable);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_MISSING),
            crosapi::mojom::ScannerOperationResult::kDeviceMissing);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_INTERNAL_ERROR),
            crosapi::mojom::ScannerOperationResult::kInternalError);
}

TEST(DocumentScanAshTypeConvertersTest,
     GetScannerListResponse_EmptyObjectSucceeds) {
  lorgnette::ListScannersResponse input;
  auto output = crosapi::mojom::GetScannerListResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_EQ(output->scanners.size(), 0U);
}

TEST(DocumentScanAshTypeConvertersTest, GetScannerListResponse_UsbScanner) {
  lorgnette::ListScannersResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  lorgnette::ScannerInfo* scanner = input.add_scanners();
  scanner->set_name("backend:usb:18d1:505e");
  scanner->set_manufacturer("GoogleTest");
  scanner->set_model("USB Scanner");
  scanner->set_display_name("GoogleTest USB Scanner (USB)");
  scanner->set_device_uuid("13245-67890");
  scanner->set_connection_type(lorgnette::CONNECTION_USB);
  scanner->set_secure(true);
  scanner->add_image_format("image/png");
  scanner->add_image_format("image/jpeg");
  scanner->set_protocol_type("backend");

  auto output = crosapi::mojom::GetScannerListResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  ASSERT_EQ(output->scanners.size(), 1U);
  crosapi::mojom::ScannerInfoPtr& scanner_out = output->scanners[0];
  EXPECT_EQ(scanner_out->id, "backend:usb:18d1:505e");
  EXPECT_EQ(scanner_out->manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out->model, "USB Scanner");
  EXPECT_EQ(scanner_out->display_name, "GoogleTest USB Scanner (USB)");
  EXPECT_EQ(scanner_out->device_uuid, "13245-67890");
  EXPECT_EQ(scanner_out->connection_type,
            crosapi::mojom::ScannerInfo_ConnectionType::kUsb);
  EXPECT_TRUE(scanner_out->secure);
  EXPECT_THAT(scanner_out->image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
  EXPECT_EQ(scanner_out->protocol_type, "backend");
}

TEST(DocumentScanAshTypeConvertersTest, GetScannerListResponse_NetworkScanner) {
  lorgnette::ListScannersResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_NO_MEMORY);
  lorgnette::ScannerInfo* scanner = input.add_scanners();
  scanner->set_name("backend:net:127.0.0.1");
  scanner->set_manufacturer("GoogleTest");
  scanner->set_model("Network Scanner");
  scanner->set_display_name("GoogleTest Network Scanner");
  scanner->set_device_uuid("13245-67890");
  scanner->set_connection_type(lorgnette::CONNECTION_NETWORK);
  scanner->set_secure(false);
  scanner->add_image_format("image/png");
  scanner->add_image_format("image/jpeg");
  scanner->set_protocol_type("backend");

  auto output = crosapi::mojom::GetScannerListResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kNoMemory);
  ASSERT_EQ(output->scanners.size(), 1U);
  crosapi::mojom::ScannerInfoPtr& scanner_out = output->scanners[0];
  EXPECT_EQ(scanner_out->id, "backend:net:127.0.0.1");
  EXPECT_EQ(scanner_out->manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out->model, "Network Scanner");
  EXPECT_EQ(scanner_out->display_name, "GoogleTest Network Scanner");
  EXPECT_EQ(scanner_out->device_uuid, "13245-67890");
  EXPECT_EQ(scanner_out->connection_type,
            crosapi::mojom::ScannerInfo_ConnectionType::kNetwork);
  EXPECT_FALSE(scanner_out->secure);
  EXPECT_THAT(scanner_out->image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
  EXPECT_EQ(scanner_out->protocol_type, "backend");
}


}  // namespace
}  // namespace mojo
