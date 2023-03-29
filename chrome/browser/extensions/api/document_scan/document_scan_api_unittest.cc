// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace api {

namespace {

// Scanner name used for tests.
constexpr char kTestScannerName[] = "Test Scanner";
constexpr char kVirtualUSBPrinterName[] = "DavieV Virtual USB Printer (USB)";

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
    if (scan_data.has_value())
      DCHECK(!scan_data.value().empty());
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

}  // namespace

class DocumentScanScanFunctionTest : public ExtensionApiUnittest {
 public:
  DocumentScanScanFunctionTest()
      : function_(base::MakeRefCounted<DocumentScanScanFunction>()) {}
  ~DocumentScanScanFunctionTest() override {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    function_->set_user_gesture(true);
    function_->SetMojoInterfaceForTesting(&document_scan_);
  }

  void TearDown() override {
    function_->SetMojoInterfaceForTesting(nullptr);
    ExtensionApiUnittest::TearDown();
  }

  TestDocumentScan& GetDocumentScan() { return document_scan_; }

 protected:
  std::string RunFunctionAndReturnError(const std::string& args) {
    function_->set_extension(extension());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function_.get(), args, browser()->profile(),
        api_test_utils::FunctionMode::kNone);
    return error;
  }

  scoped_refptr<DocumentScanScanFunction> function_;
  TestDocumentScan document_scan_;
};

TEST_F(DocumentScanScanFunctionTest, UserGestureRequiredError) {
  function_->set_user_gesture(false);
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, NoScannersAvailableError) {
  GetDocumentScan().SetGetScannerNamesResponse({});
  EXPECT_EQ("No scanners available", RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, UnsupportedMimeTypesError) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  EXPECT_EQ("Unsupported MIME types",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"image/tiff\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, ScanImageError) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  GetDocumentScan().SetScanResponse(absl::nullopt);
  EXPECT_EQ("Failed to scan image",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"image/png\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, Success) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  const std::vector<std::string> scan_data = {"PrettyPicture"};
  GetDocumentScan().SetScanResponse(scan_data);
  absl::optional<base::Value::Dict> result = RunFunctionAndReturnDictionary(
      function_.get(), "[{\"mimeTypes\": [\"image/png\"]}]");
  ASSERT_TRUE(result);
  document_scan::ScanResults scan_results;
  EXPECT_TRUE(document_scan::ScanResults::Populate(*result, scan_results));
  // Verify the image data URL is the PNG image data URL prefix plus the base64
  // representation of "PrettyPicture".
  EXPECT_THAT(
      scan_results.data_urls,
      testing::ElementsAre("data:image/png;base64,UHJldHR5UGljdHVyZQ=="));
  EXPECT_EQ("image/png", scan_results.mime_type);
}

TEST_F(DocumentScanScanFunctionTest, TestingMIMETypeError) {
  GetDocumentScan().SetGetScannerNamesResponse({kTestScannerName});
  EXPECT_EQ("Virtual USB printer unavailable",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"testing\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, TestingMIMEType) {
  GetDocumentScan().SetGetScannerNamesResponse(
      {kTestScannerName, kVirtualUSBPrinterName});
  const std::vector<std::string> scan_data = {"PrettyPicture"};
  GetDocumentScan().SetScanResponse(scan_data);
  absl::optional<base::Value::Dict> result = RunFunctionAndReturnDictionary(
      function_.get(), "[{\"mimeTypes\": [\"testing\"]}]");
  ASSERT_TRUE(result);
  document_scan::ScanResults scan_results;
  EXPECT_TRUE(document_scan::ScanResults::Populate(*result, scan_results));
  // Verify the image data URL is the PNG image data URL prefix plus the base64
  // representation of "PrettyPicture".
  EXPECT_THAT(
      scan_results.data_urls,
      testing::ElementsAre("data:image/png;base64,UHJldHR5UGljdHVyZQ=="));
  EXPECT_EQ("image/png", scan_results.mime_type);
}

}  // namespace api

}  // namespace extensions
