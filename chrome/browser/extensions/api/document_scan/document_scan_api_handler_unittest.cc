// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

using SimpleScanFuture =
    base::test::TestFuture<absl::optional<api::document_scan::ScanResults>,
                           absl::optional<std::string>>;

// Scanner name used for tests.
constexpr char kTestScannerName[] = "Test Scanner";
constexpr char kVirtualUSBPrinterName[] = "DavieV Virtual USB Printer (USB)";

// Fake scan data.
constexpr char kScanDataItem[] = "PrettyPicture";
constexpr char kScanDataItemBase64[] =
    "data:image/png;base64,UHJldHR5UGljdHVyZQ==";

class TestDocumentScan : public crosapi::mojom::DocumentScan {
 public:
  TestDocumentScan() = default;
  TestDocumentScan(const TestDocumentScan&) = delete;
  TestDocumentScan& operator=(const TestDocumentScan&) = delete;
  ~TestDocumentScan() override = default;

  void SetGetScannerNamesResponse(std::vector<std::string> scanner_names) {
    scanner_names_ = std::move(scanner_names);
  }

  void SetScanResponse(
      const absl::optional<std::vector<std::string>>& scan_data) {
    if (scan_data.has_value()) {
      DCHECK(!scan_data.value().empty());
    }
    scan_data_ = scan_data;
  }

  // crosapi::mojom::DocumentScan:
  void GetScannerNames(GetScannerNamesCallback callback) override {
    std::move(callback).Run(scanner_names_);
  }
  void ScanFirstPage(const std::string& scanner_name,
                     ScanFirstPageCallback callback) override {
    if (scan_data_.has_value()) {
      std::move(callback).Run(crosapi::mojom::ScanFailureMode::kNoFailure,
                              scan_data_.value()[0]);
    } else {
      std::move(callback).Run(crosapi::mojom::ScanFailureMode::kDeviceBusy,
                              absl::nullopt);
    }
  }

 private:
  std::vector<std::string> scanner_names_;
  absl::optional<std::vector<std::string>> scan_data_;
};

class DocumentScanAPIHandlerTest : public testing::Test {
 public:
  DocumentScanAPIHandlerTest() = default;
  DocumentScanAPIHandlerTest(const DocumentScanAPIHandlerTest&) = delete;
  DocumentScanAPIHandlerTest& operator=(const DocumentScanAPIHandlerTest&) =
      delete;
  ~DocumentScanAPIHandlerTest() override = default;

  void SetUp() override {
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    testing_profile_ =
        profile_manager_->CreateTestingProfile(chrome::kInitialProfile);

    document_scan_api_handler_ = DocumentScanAPIHandler::CreateForTesting(
        testing_profile_, &document_scan_);
  }

  void TearDown() override {
    document_scan_api_handler_.reset();
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  TestDocumentScan& GetDocumentScan() { return document_scan_; }

 protected:
  std::unique_ptr<DocumentScanAPIHandler> document_scan_api_handler_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> testing_profile_;
  TestDocumentScan document_scan_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
};

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_NoScannersAvailableError) {
  GetDocumentScan().SetGetScannerNamesResponse({});
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({"image/png"}, future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(scan_results.has_value());
  EXPECT_EQ("No scanners available", error);
}

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_MissingMimeTypesError) {
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({}, future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(scan_results.has_value());
  EXPECT_EQ("Unsupported MIME types", error);
}

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_UnsupportedMimeTypesError) {
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({"image/tiff"}, future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(scan_results.has_value());
  EXPECT_EQ("Unsupported MIME types", error);
}

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_ScanImageError) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  GetDocumentScan().SetScanResponse(absl::nullopt);
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({"image/png"}, future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(scan_results.has_value());
  EXPECT_EQ("Failed to scan image", error);
}

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_Success) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  const std::vector<std::string> scan_data = {kScanDataItem};
  GetDocumentScan().SetScanResponse(scan_data);
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({"image/png"}, future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(error.has_value());
  ASSERT_TRUE(scan_results.has_value());

  // Verify the image data URL is the PNG image data URL prefix plus the base64
  // representation of "PrettyPicture".
  EXPECT_THAT(scan_results->data_urls,
              testing::ElementsAre(kScanDataItemBase64));
  EXPECT_EQ("image/png", scan_results->mime_type);
}

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_TestingMIMETypeError) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({"testing"}, future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(scan_results.has_value());
  EXPECT_EQ("Virtual USB printer unavailable", error);
}

TEST_F(DocumentScanAPIHandlerTest, SimpleScan_TestingMIMETypeSuccess) {
  GetDocumentScan().SetGetScannerNamesResponse(
      {kTestScannerName, kVirtualUSBPrinterName});
  const std::vector<std::string> scan_data = {kScanDataItem};
  GetDocumentScan().SetScanResponse(scan_data);
  SimpleScanFuture future;
  document_scan_api_handler_->SimpleScan({"image/png", "testing"},
                                         future.GetCallback());
  const auto& [scan_results, error] = future.Get();
  EXPECT_FALSE(error.has_value());
  ASSERT_TRUE(scan_results.has_value());

  // Verify the image data URL is the PNG image data URL prefix plus the base64
  // representation of "PrettyPicture".
  EXPECT_THAT(scan_results->data_urls,
              testing::ElementsAre(kScanDataItemBase64));
  EXPECT_EQ("image/png", scan_results->mime_type);
}

}  // namespace

}  // namespace extensions
