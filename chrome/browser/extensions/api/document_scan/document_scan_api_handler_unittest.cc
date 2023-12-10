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
using OpenScannerFuture =
    base::test::TestFuture<api::document_scan::OpenScannerResponse>;
using CloseScannerFuture =
    base::test::TestFuture<api::document_scan::CloseScannerResponse>;

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
  void MarkExtensionTrusted(const ExtensionId extension_id) {
    testing_profile_->GetTestingPrefService()->SetList(
        prefs::kDocumentScanAPITrustedExtensions,
        base::Value::List().Append(extension_id));
  }

  // "Discover" a scanner and return its unguessable token.  After calling this,
  // a test can use the returned scanner ID to open a scanner for further
  // operations.
  absl::optional<std::string> CreateScannerIdForExtension(
      scoped_refptr<const Extension> extension) {
    GetDocumentScan().AddScanner(CreateTestScannerInfo());
    MarkExtensionTrusted(extension->id());

    GetScannerListFuture list_future;
    document_scan_api_handler_->GetScannerList(
        /*native_window=*/nullptr, extension, {}, list_future.GetCallback());
    const api::document_scan::GetScannerListResponse& list_response =
        list_future.Get();
    if (list_response.scanners.empty()) {
      return absl::nullopt;
    }
    return list_response.scanners[0].scanner_id;
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
  MarkExtensionTrusted(kExtensionId);
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
  MarkExtensionTrusted(kExtensionId);

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

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_OpenBeforeListFails) {
  OpenScannerFuture future;
  document_scan_api_handler_->OpenScanner(extension_, "badscanner",
                                          future.GetCallback());
  const api::document_scan::OpenScannerResponse& response = future.Get();

  EXPECT_EQ(response.scanner_id, "badscanner");
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_FALSE(response.scanner_handle.has_value());
  EXPECT_FALSE(response.options.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_OpenInvalidFails) {
  std::string scanner_id = CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id.empty());

  // Calling GetScannerList invalidates the previously returned ID.
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, {}, base::DoNothing());

  OpenScannerFuture future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future.GetCallback());
  const api::document_scan::OpenScannerResponse& response = future.Get();

  EXPECT_EQ(response.scanner_id, scanner_id);
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_FALSE(response.scanner_handle.has_value());
  EXPECT_FALSE(response.options.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_ReopenValidIdSucceeds) {
  std::string scanner_id = CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id.empty());

  // The first open succeeds because the scanner is not open.
  OpenScannerFuture future1;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future1.GetCallback());
  const api::document_scan::OpenScannerResponse& response1 = future1.Get();

  // The second open succeeds because this is the same extension.
  OpenScannerFuture future2;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future2.GetCallback());
  const api::document_scan::OpenScannerResponse& response2 = future2.Get();

  EXPECT_EQ(response1.scanner_id, scanner_id);
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response1.scanner_handle.has_value());
  EXPECT_FALSE(response1.scanner_handle->empty());
  ASSERT_TRUE(response1.options.has_value());
  EXPECT_TRUE(response1.options->additional_properties.contains("option1"));

  EXPECT_EQ(response2.scanner_id, scanner_id);
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response2.scanner_handle.has_value());
  EXPECT_FALSE(response2.scanner_handle->empty());
  ASSERT_TRUE(response2.options.has_value());
  EXPECT_TRUE(response2.options->additional_properties.contains("option1"));
}

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_ReopenSameScannerSucceeds) {
  std::string scanner_id = CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id.empty());

  // The first open succeeds because the scanner is not open.
  OpenScannerFuture future1;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future1.GetCallback());
  const api::document_scan::OpenScannerResponse& response1 = future1.Get();

  // GetScannerList returns a new token that points to the same underlying
  // scanner created by CreateScannerId above.
  std::string scanner_id2 =
      CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id2.empty());

  // Opening the second ID succeeds because this is the same extension.
  OpenScannerFuture future2;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id2,
                                          future2.GetCallback());
  const api::document_scan::OpenScannerResponse& response2 = future2.Get();

  EXPECT_EQ(response1.scanner_id, scanner_id);
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response1.scanner_handle.has_value());
  EXPECT_FALSE(response1.scanner_handle->empty());
  ASSERT_TRUE(response1.options.has_value());
  EXPECT_TRUE(response1.options->additional_properties.contains("option1"));

  EXPECT_EQ(response2.scanner_id, scanner_id2);
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response2.scanner_handle.has_value());
  EXPECT_FALSE(response2.scanner_handle->empty());
  ASSERT_TRUE(response2.options.has_value());
  EXPECT_TRUE(response2.options->additional_properties.contains("option1"));
}

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_SecondExtensionOpenFails) {
  std::string scanner_id = CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id.empty());

  // The first open succeeds because the scanner is not open.
  OpenScannerFuture open_future1;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future1.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response1 =
      open_future1.Get();

  // Get a new token for the second extension that points to the same underlying
  // scanner created by CreateScannerId above.
  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddPermission(kExtensionPermissionName)
                        .Build();
  std::string scanner_id2 =
      CreateScannerIdForExtension(extension2).value_or("");
  ASSERT_FALSE(scanner_id2.empty());

  // Opening the same scanner from a second extension fails because the scanner
  // is already open.
  OpenScannerFuture open_future2;
  document_scan_api_handler_->OpenScanner(extension2, scanner_id2,
                                          open_future2.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response2 =
      open_future2.Get();

  EXPECT_EQ(open_response1.scanner_id, scanner_id);
  EXPECT_EQ(open_response1.result,
            api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(open_response1.scanner_handle.has_value());
  EXPECT_FALSE(open_response1.scanner_handle->empty());
  ASSERT_TRUE(open_response1.options.has_value());
  EXPECT_TRUE(
      open_response1.options->additional_properties.contains("option1"));

  EXPECT_EQ(open_response2.scanner_id, scanner_id2);
  EXPECT_EQ(open_response2.result,
            api::document_scan::OperationResult::kDeviceBusy);
  EXPECT_FALSE(open_response2.scanner_handle.has_value());
  EXPECT_FALSE(open_response2.options.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, CloseScanner_CloseBeforeOpenFails) {
  CloseScannerFuture future;
  document_scan_api_handler_->CloseScanner(extension_, "badscanner",
                                           future.GetCallback());
  const api::document_scan::CloseScannerResponse& response = future.Get();

  EXPECT_EQ(response.scanner_handle, "badscanner");
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
}

TEST_F(DocumentScanAPIHandlerTest, CloseScanner_CloseInvalidHandleFails) {
  std::string scanner_id = CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id.empty());

  OpenScannerFuture open_future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response =
      open_future.Get();
  ASSERT_TRUE(open_response.scanner_handle.has_value());
  const std::string& handle = open_response.scanner_handle.value();

  // Closing a valid handle from a different extension fails because it isn't
  // valid for the second extension.
  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddPermission(kExtensionPermissionName)
                        .Build();

  CloseScannerFuture close_future1;
  document_scan_api_handler_->CloseScanner(extension2, handle,
                                           close_future1.GetCallback());
  const api::document_scan::CloseScannerResponse& close_response1 =
      close_future1.Get();
  EXPECT_EQ(close_response1.scanner_handle, handle);
  EXPECT_EQ(close_response1.result,
            api::document_scan::OperationResult::kInvalid);

  // Closing the handle from the original extension still succeeds.
  CloseScannerFuture close_future2;
  document_scan_api_handler_->CloseScanner(extension_, handle,
                                           close_future2.GetCallback());
  const api::document_scan::CloseScannerResponse& close_response2 =
      close_future2.Get();
  EXPECT_EQ(close_response2.scanner_handle, handle);
  EXPECT_EQ(close_response2.result,
            api::document_scan::OperationResult::kSuccess);
}

TEST_F(DocumentScanAPIHandlerTest, CloseScanner_DoubleCloseHandleFails) {
  std::string scanner_id = CreateScannerIdForExtension(extension_).value_or("");
  ASSERT_FALSE(scanner_id.empty());

  OpenScannerFuture open_future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response =
      open_future.Get();
  ASSERT_TRUE(open_response.scanner_handle.has_value());
  const std::string& handle = open_response.scanner_handle.value();

  // First call succeeds because the handle is valid.
  CloseScannerFuture close_future1;
  document_scan_api_handler_->CloseScanner(extension_, handle,
                                           close_future1.GetCallback());
  const api::document_scan::CloseScannerResponse& close_response1 =
      close_future1.Get();
  EXPECT_EQ(close_response1.scanner_handle, handle);
  EXPECT_EQ(close_response1.result,
            api::document_scan::OperationResult::kSuccess);

  // Closing the same handle again fails.
  CloseScannerFuture close_future2;
  document_scan_api_handler_->CloseScanner(extension_, handle,
                                           close_future2.GetCallback());
  const api::document_scan::CloseScannerResponse& close_response2 =
      close_future2.Get();
  EXPECT_EQ(close_response2.scanner_handle, handle);
  EXPECT_EQ(close_response2.result,
            api::document_scan::OperationResult::kInvalid);
}

}  // namespace
}  // namespace extensions
