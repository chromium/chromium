// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

namespace document_scan = extensions::api::document_scan;
namespace mojom = crosapi::mojom;

using ::testing::UnorderedElementsAre;

TEST(DocumentScanTypeConvertersTest, OperationResult) {
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kUnknown),
            document_scan::OperationResult::kUnknown);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kSuccess),
            document_scan::OperationResult::kSuccess);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kUnsupported),
            document_scan::OperationResult::kUnsupported);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kCancelled),
            document_scan::OperationResult::kCancelled);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kDeviceBusy),
            document_scan::OperationResult::kDeviceBusy);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kInvalid),
            document_scan::OperationResult::kInvalid);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kWrongType),
            document_scan::OperationResult::kWrongType);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kEndOfData),
            document_scan::OperationResult::kEof);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kAdfJammed),
            document_scan::OperationResult::kAdfJammed);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kAdfEmpty),
            document_scan::OperationResult::kAdfEmpty);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kCoverOpen),
            document_scan::OperationResult::kCoverOpen);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kIoError),
            document_scan::OperationResult::kIoError);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kAccessDenied),
            document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kNoMemory),
            document_scan::OperationResult::kNoMemory);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kDeviceUnreachable),
            document_scan::OperationResult::kUnreachable);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kDeviceMissing),
            document_scan::OperationResult::kMissing);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kInternalError),
            document_scan::OperationResult::kInternalError);
}

TEST(DocumentScanTypeConvertersTest, DeviceFilter_Empty) {
  document_scan::DeviceFilter input;
  auto output = mojom::ScannerEnumFilter::From(input);
  EXPECT_FALSE(output->local);
  EXPECT_FALSE(output->secure);
}

TEST(DocumentScanTypeConvertersTest, DeviceFilter_Local) {
  document_scan::DeviceFilter input;
  input.local = true;
  auto output = mojom::ScannerEnumFilter::From(input);
  EXPECT_TRUE(output->local);
  EXPECT_FALSE(output->secure);
}

TEST(DocumentScanTypeConvertersTest, DeviceFilter_Secure) {
  document_scan::DeviceFilter input;
  input.secure = true;
  auto output = mojom::ScannerEnumFilter::From(input);
  EXPECT_FALSE(output->local);
  EXPECT_TRUE(output->secure);
}

TEST(DocumentScanTypeConvertersTest, GetScannerListResponse_Empty) {
  auto input = mojom::GetScannerListResponse::New();
  auto output = input.To<document_scan::GetScannerListResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_EQ(output.scanners.size(), 0U);
}

TEST(DocumentScanTypeConvertersTest, GetScannerListResponse_Usb) {
  auto input = mojom::GetScannerListResponse::New();
  input->result = mojom::ScannerOperationResult::kSuccess;
  auto scanner_in = mojom::ScannerInfo::New();
  scanner_in->id = "12345";
  scanner_in->display_name = "12345 (USB)";
  scanner_in->manufacturer = "GoogleTest";
  scanner_in->model = "USB Scanner";
  scanner_in->device_uuid = "56789";
  scanner_in->connection_type = mojom::ScannerInfo_ConnectionType::kUsb;
  scanner_in->secure = true;
  scanner_in->image_formats = {"image/png", "image/jpeg"};
  input->scanners.emplace_back(std::move(scanner_in));

  auto output = input.To<document_scan::GetScannerListResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  ASSERT_EQ(output.scanners.size(), 1U);
  const document_scan::ScannerInfo& scanner_out = output.scanners[0];
  EXPECT_EQ(scanner_out.scanner_id, "12345");
  EXPECT_EQ(scanner_out.name, "12345 (USB)");
  EXPECT_EQ(scanner_out.manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out.model, "USB Scanner");
  EXPECT_EQ(scanner_out.device_uuid, "56789");
  EXPECT_EQ(scanner_out.connection_type, document_scan::ConnectionType::kUsb);
  EXPECT_EQ(scanner_out.secure, true);
  EXPECT_THAT(scanner_out.image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
}

TEST(DocumentScanTypeConvertersTest, GetScannerListResponse_Network) {
  auto input = mojom::GetScannerListResponse::New();
  input->result = mojom::ScannerOperationResult::kNoMemory;
  auto scanner_in = mojom::ScannerInfo::New();
  scanner_in->id = "12345";
  scanner_in->display_name = "12345";
  scanner_in->manufacturer = "GoogleTest";
  scanner_in->model = "Network Scanner";
  scanner_in->device_uuid = "56789";
  scanner_in->connection_type = mojom::ScannerInfo_ConnectionType::kNetwork;
  scanner_in->secure = true;
  scanner_in->image_formats = {"image/png", "image/jpeg"};
  input->scanners.emplace_back(std::move(scanner_in));

  auto output = input.To<document_scan::GetScannerListResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kNoMemory);
  ASSERT_EQ(output.scanners.size(), 1U);
  const document_scan::ScannerInfo& scanner_out = output.scanners[0];
  EXPECT_EQ(scanner_out.scanner_id, "12345");
  EXPECT_EQ(scanner_out.name, "12345");
  EXPECT_EQ(scanner_out.manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out.model, "Network Scanner");
  EXPECT_EQ(scanner_out.device_uuid, "56789");
  EXPECT_EQ(scanner_out.connection_type,
            document_scan::ConnectionType::kNetwork);
  EXPECT_EQ(scanner_out.secure, true);
  EXPECT_THAT(scanner_out.image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
}

}  // namespace
}  // namespace mojo
