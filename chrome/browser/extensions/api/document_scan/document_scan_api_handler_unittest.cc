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
#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"
#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/document_scan.mojom.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

namespace {

using SimpleScanFuture =
    base::test::TestFuture<absl::optional<api::document_scan::ScanResults>,
                           absl::optional<std::string>>;

using GetScannerListFuture =
    base::test::TestFuture<api::document_scan::GetScannerListResponse>;

// Fake extension info.
constexpr char kExtensionId[] = "abcdefghijklmnopqrstuvwxyz123456";
constexpr char kExtensionName[] = "DocumentScan API extension";
constexpr char kExtensionPermissionName[] = "documentScan";

// Scanner name used for tests.
constexpr char kTestScannerName[] = "Test Scanner";
constexpr char kVirtualUSBPrinterName[] = "DavieV Virtual USB Printer (USB)";

// Fake scan data.
constexpr char kScanDataItem[] = "PrettyPicture";
constexpr char kScanDataItemBase64[] =
    "data:image/png;base64,UHJldHR5UGljdHVyZQ==";

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

    extension_ = ExtensionBuilder(kExtensionName)
                     .SetID(kExtensionId)
                     .AddPermission(kExtensionPermissionName)
                     .Build();
    ExtensionRegistry::Get(testing_profile_)->AddEnabled(extension_);

    document_scan_api_handler_ = DocumentScanAPIHandler::CreateForTesting(
        testing_profile_, &document_scan_);
  }

  void TearDown() override {
    document_scan_api_handler_.reset();
    testing_profile_ = nullptr;
    profile_manager_->DeleteTestingProfile(chrome::kInitialProfile);
  }

  FakeDocumentScanAsh& GetDocumentScan() { return document_scan_; }

 protected:
  std::unique_ptr<DocumentScanAPIHandler> document_scan_api_handler_;
  scoped_refptr<const Extension> extension_;

  // Allow an extension to bypass user confirmation dialogs by adding it to the
  // list of trusted document scan extensions.
  void MarkExtensionTrusted() {
    testing_profile_->GetTestingPrefService()->SetList(
        prefs::kDocumentScanAPITrustedExtensions,
        base::Value::List().Append(kExtensionId));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  raw_ptr<TestingProfile> testing_profile_;
  FakeDocumentScanAsh document_scan_;
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

TEST_F(DocumentScanAPIHandlerTest, GetScannerList_DiscoveryDenied) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  api::document_scan::DeviceFilter filter;
  GetScannerListFuture future;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, std::move(filter),
      future.GetCallback());

  const api::document_scan::GetScannerListResponse& response = future.Get();
  EXPECT_EQ(response.result,
            api::document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(response.scanners.size(), 0U);
}

TEST_F(DocumentScanAPIHandlerTest, GetScannerList_DiscoveryApproved) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(true);
  api::document_scan::DeviceFilter filter;
  GetScannerListFuture future;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, std::move(filter),
      future.GetCallback());

  const api::document_scan::GetScannerListResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response.scanners.size(), 1U);
  EXPECT_EQ(response.scanners[0].model, "Scanner");
}

TEST_F(DocumentScanAPIHandlerTest, GetScannerList_DiscoveryTrusted) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());
  MarkExtensionTrusted();
  // Confirmation will be denied, but it won't matter because the extension is
  // trusted.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);

  api::document_scan::DeviceFilter filter;
  GetScannerListFuture future;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, std::move(filter),
      future.GetCallback());

  const api::document_scan::GetScannerListResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response.scanners.size(), 1U);
  EXPECT_EQ(response.scanners[0].model, "Scanner");
}

TEST_F(DocumentScanAPIHandlerTest, GetScannerList_NewIdBetweenCalls) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());
  MarkExtensionTrusted();

  api::document_scan::DeviceFilter filter1;
  GetScannerListFuture future1;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, std::move(filter1),
      future1.GetCallback());

  api::document_scan::DeviceFilter filter2;
  GetScannerListFuture future2;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, std::move(filter2),
      future2.GetCallback());

  const api::document_scan::GetScannerListResponse& response1 = future1.Get();
  const api::document_scan::GetScannerListResponse& response2 = future2.Get();
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response1.scanners.size(), 1U);
  ASSERT_EQ(response2.scanners.size(), 1U);
  EXPECT_NE(response1.scanners[0].scanner_id, response2.scanners[0].scanner_id);
}

}  // namespace

}  // namespace extensions
