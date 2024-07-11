// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api_handler.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"
#include "chrome/browser/extensions/api/document_scan/fake_document_scan_ash.h"
#include "chrome/browser/extensions/api/document_scan/scanner_discovery_runner.h"
#include "chrome/browser/extensions/api/document_scan/start_scan_runner.h"
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
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_builder.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using SimpleScanFuture =
    base::test::TestFuture<std::optional<api::document_scan::ScanResults>,
                           std::optional<std::string>>;

using GetScannerListFuture =
    base::test::TestFuture<api::document_scan::GetScannerListResponse>;
using OpenScannerFuture =
    base::test::TestFuture<api::document_scan::OpenScannerResponse>;
using GetOptionGroupsFuture =
    base::test::TestFuture<api::document_scan::GetOptionGroupsResponse>;
using CloseScannerFuture =
    base::test::TestFuture<api::document_scan::CloseScannerResponse>;
using SetOptionsFuture =
    base::test::TestFuture<api::document_scan::SetOptionsResponse>;
using StartScanFuture =
    base::test::TestFuture<api::document_scan::StartScanResponse>;
using CancelScanFuture =
    base::test::TestFuture<api::document_scan::CancelScanResponse>;
using ReadScanDataFuture =
    base::test::TestFuture<api::document_scan::ReadScanDataResponse>;

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
                     .AddAPIPermission(kExtensionPermissionName)
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

  // Send a notification that `extension` has been unloaded.
  void UnloadExtension() {
    document_scan_api_handler_->OnExtensionUnloaded(
        testing_profile_, extension_.get(), UnloadedExtensionReason::UNINSTALL);
  }

  // Allow an extension to bypass user confirmation dialogs by adding it to the
  // list of trusted document scan extensions.
  void MarkExtensionTrusted(const ExtensionId extension_id) {
    testing_profile_->GetTestingPrefService()->SetList(
        prefs::kDocumentScanAPITrustedExtensions,
        base::Value::List().Append(extension_id));
  }

  // "Discover" a scanner and return its unguessable token.  After calling this,
  // a test can use the returned scanner ID to open a scanner for further
  // operations.  If `unique_id` is true, a unique scanner ID will be used for
  // the scanner that gets created.  Otherwise, a constant ID will be used.
  std::string CreateScannerIdForExtension(
      scoped_refptr<const Extension> extension,
      bool unique_id = true) {
    auto scanner_info = CreateTestScannerInfo();
    if (unique_id) {
      static size_t counter = 0;
      scanner_info->id =
          base::StringPrintf("%s-%zu", scanner_info->id.c_str(), counter++);
    }
    const std::string the_scanner_id = scanner_info->id;
    GetDocumentScan().AddScanner(std::move(scanner_info));
    ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(true);

    GetScannerListFuture list_future;
    document_scan_api_handler_->GetScannerList(
        /*native_window=*/nullptr, extension, /*user_gesture=*/false, {},
        list_future.GetCallback());
    const api::document_scan::GetScannerListResponse& list_response =
        list_future.Get();

    for (const auto& scanner : list_response.scanners) {
      if (scanner.scanner_id == the_scanner_id) {
        return scanner.scanner_id;
      }
    }

    return "";
  }

  // "Discover" a scanner and open that given scanner, returning the scanner
  // handle.  After calling this, a test can use the returned scanner handle for
  // further operations.  Note that this will always use a unique scanner ID for
  // the scanner that is created.
  std::string OpenScannerForExtension(
      scoped_refptr<const Extension> extension) {
    return OpenScannerWithId(extension, CreateScannerIdForExtension(extension));
  }

  // "Discover" and open a scanner, start a scan on that scanner, and return the
  // job handle.  After calling this, a test can use the returned job handle for
  // further operations.
  std::string StartScanForExtension(scoped_refptr<const Extension> extension) {
    return StartScanForScannerHandle(extension, /*user_gesture=*/false,
                                     OpenScannerForExtension(extension));
  }

  // Open the scanner with the given `scanner_id`, returning the scanner handle.
  // This requires that the scanner has already been added to the fake document
  // scan object.  After calling this, a test can use the returned scanner
  // handle for further operations.
  std::string OpenScannerWithId(scoped_refptr<const Extension> extension,
                                const std::string& scanner_id) {
    if (scanner_id.empty()) {
      return "";
    }

    OpenScannerFuture future;
    document_scan_api_handler_->OpenScanner(extension, scanner_id,
                                            future.GetCallback());
    const api::document_scan::OpenScannerResponse& response = future.Get();
    return response.scanner_handle.value_or("");
  }

  // Start a scan for the scanner given by `scanner_handle`, returning the job
  // handle.  This requires that the scanner has already been added to the fake
  // document scan object.  After calling this, a test can use the returned job
  // handle for further operations.
  std::string StartScanForScannerHandle(
      scoped_refptr<const Extension> extension,
      bool user_gesture,
      const std::string& scanner_handle) {
    if (scanner_handle.empty()) {
      return "";
    }

    base::AutoReset<std::optional<bool>> testing_scope =
        StartScanRunner::SetStartScanConfirmationResultForTesting(true);
    api::document_scan::StartScanOptions options;
    StartScanFuture future;
    document_scan_api_handler_->StartScan(
        /*native_window=*/nullptr, extension, user_gesture, scanner_handle,
        std::move(options), future.GetCallback());

    const api::document_scan::StartScanResponse& response = future.Get();
    return response.job.value_or("");
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
  GetDocumentScan().SetScanResponse(std::nullopt);
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
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future.GetCallback());

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
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future.GetCallback());

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
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future.GetCallback());

  const api::document_scan::GetScannerListResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response.scanners.size(), 1U);
  EXPECT_EQ(response.scanners[0].model, "Scanner");
}

TEST_F(DocumentScanAPIHandlerTest,
       GetScannerList_MultipleDiscoveryPreservesApproval) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());

  // First request is denied.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  api::document_scan::DeviceFilter filter;
  GetScannerListFuture future1;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future1.GetCallback());
  const api::document_scan::GetScannerListResponse& response1 = future1.Get();
  EXPECT_EQ(response1.result,
            api::document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(response1.scanners.size(), 0U);

  // Second request is approved by the user.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(true);
  GetScannerListFuture future2;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future2.GetCallback());
  const api::document_scan::GetScannerListResponse& response2 = future2.Get();
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response2.scanners.size(), 1U);
  EXPECT_EQ(response2.scanners[0].model, "Scanner");

  // After approval, user-initiated request is approved without showing the
  // dialog.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  GetScannerListFuture future3;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      std::move(filter), future3.GetCallback());
  const api::document_scan::GetScannerListResponse& response3 = future3.Get();
  EXPECT_EQ(response3.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response3.scanners.size(), 1U);
  EXPECT_EQ(response3.scanners[0].model, "Scanner");

  // Unsolicited request is denied in spite of previous approval.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  GetScannerListFuture future4;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future4.GetCallback());
  const api::document_scan::GetScannerListResponse& response4 = future4.Get();
  EXPECT_EQ(response4.result,
            api::document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(response4.scanners.size(), 0U);

  // User-initiated request is no longer automatically approved because of
  // previous denial.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  GetScannerListFuture future5;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      std::move(filter), future5.GetCallback());
  const api::document_scan::GetScannerListResponse& response5 = future5.Get();
  EXPECT_EQ(response5.result,
            api::document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(response5.scanners.size(), 0U);
}

TEST_F(DocumentScanAPIHandlerTest, GetScannerList_ApprovalFollowsExtension) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());

  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddAPIPermission(kExtensionPermissionName)
                        .Build();

  // First request is approved by the user.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(true);
  api::document_scan::DeviceFilter filter;
  GetScannerListFuture future1;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter), future1.GetCallback());
  const api::document_scan::GetScannerListResponse& response1 = future1.Get();
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response1.scanners.size(), 1U);
  EXPECT_EQ(response1.scanners[0].model, "Scanner");

  // First extension's saved approval doesn't apply to second extension.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  GetScannerListFuture future2;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension2, /*user_gesture=*/true,
      std::move(filter), future2.GetCallback());
  const api::document_scan::GetScannerListResponse& response2 = future2.Get();
  EXPECT_EQ(response2.result,
            api::document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(response2.scanners.size(), 0U);

  // First extension's second request uses saved approval.
  ScannerDiscoveryRunner::SetDiscoveryConfirmationResultForTesting(false);
  GetScannerListFuture future3;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      std::move(filter), future3.GetCallback());
  const api::document_scan::GetScannerListResponse& response3 = future3.Get();
  EXPECT_EQ(response3.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response3.scanners.size(), 1U);
  EXPECT_EQ(response3.scanners[0].model, "Scanner");
}

TEST_F(DocumentScanAPIHandlerTest, GetScannerList_SameIdBetweenCalls) {
  GetDocumentScan().AddScanner(CreateTestScannerInfo());
  MarkExtensionTrusted(kExtensionId);

  api::document_scan::DeviceFilter filter1;
  GetScannerListFuture future1;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter1), future1.GetCallback());

  // Since the DocumentScanAsh service hasn't changed, the same ID should come
  // back for the same device.
  api::document_scan::DeviceFilter filter2;
  GetScannerListFuture future2;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      std::move(filter2), future2.GetCallback());

  const api::document_scan::GetScannerListResponse& response1 = future1.Get();
  const api::document_scan::GetScannerListResponse& response2 = future2.Get();
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_EQ(response1.scanners.size(), 1U);
  ASSERT_EQ(response2.scanners.size(), 1U);
  EXPECT_EQ(response1.scanners[0].scanner_id, response2.scanners[0].scanner_id);
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
  std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  // The extension got back a valid ID, but tries to open a different one.
  OpenScannerFuture future;
  std::string bad_id = scanner_id + "_invalid";
  document_scan_api_handler_->OpenScanner(extension_, bad_id,
                                          future.GetCallback());
  const api::document_scan::OpenScannerResponse& response = future.Get();

  EXPECT_EQ(response.scanner_id, bad_id);
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_FALSE(response.scanner_handle.has_value());
  EXPECT_FALSE(response.options.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_ReopenValidIdSucceeds) {
  std::string scanner_id = CreateScannerIdForExtension(extension_);
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
  std::string scanner_id = CreateScannerIdForExtension(extension_,
                                                       /*unique_id=*/false);
  ASSERT_FALSE(scanner_id.empty());

  // The first open succeeds because the scanner is not open.
  OpenScannerFuture future1;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future1.GetCallback());
  const api::document_scan::OpenScannerResponse& response1 = future1.Get();

  // GetScannerList returns a new token that points to the same underlying
  // scanner created by CreateScannerId above.
  std::string scanner_id2 = CreateScannerIdForExtension(extension_,
                                                        /*unique_id=*/false);
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
  std::string scanner_id = CreateScannerIdForExtension(extension_,
                                                       /*unique_id=*/false);
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
                        .AddAPIPermission(kExtensionPermissionName)
                        .Build();
  std::string scanner_id2 = CreateScannerIdForExtension(extension2,
                                                        /*unique_id=*/false);
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

TEST_F(DocumentScanAPIHandlerTest, OpenScanner_SecondOpenClosesFirstHandle) {
  const std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  // The first open succeeds because the scanner is not open.
  OpenScannerFuture future1;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future1.GetCallback());
  const api::document_scan::OpenScannerResponse& response1 = future1.Get();
  EXPECT_EQ(response1.scanner_id, scanner_id);
  EXPECT_EQ(response1.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response1.scanner_handle.has_value());
  EXPECT_FALSE(response1.scanner_handle->empty());
  ASSERT_TRUE(response1.options.has_value());
  EXPECT_TRUE(response1.options->additional_properties.contains("option1"));

  // First GetOptionGroups succeeds because the scanner is open.
  GetOptionGroupsFuture options_future1;
  document_scan_api_handler_->GetOptionGroups(
      extension_, *response1.scanner_handle, options_future1.GetCallback());
  const api::document_scan::GetOptionGroupsResponse& options_response1 =
      options_future1.Get();
  EXPECT_EQ(options_response1.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_TRUE(options_response1.groups.has_value());

  // The second open succeeds because this is the same extension.  This
  // implicitly closes the first handle.
  OpenScannerFuture future2;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          future2.GetCallback());
  const api::document_scan::OpenScannerResponse& response2 = future2.Get();
  EXPECT_EQ(response2.scanner_id, scanner_id);
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response2.scanner_handle.has_value());
  EXPECT_FALSE(response2.scanner_handle->empty());
  ASSERT_TRUE(response2.options.has_value());
  EXPECT_TRUE(response2.options->additional_properties.contains("option1"));

  // Second GetOptionGroups for the original handle fails because the handle is
  // now invalid.
  GetOptionGroupsFuture options_future2;
  document_scan_api_handler_->GetOptionGroups(
      extension_, *response1.scanner_handle, options_future2.GetCallback());
  const api::document_scan::GetOptionGroupsResponse& options_response2 =
      options_future2.Get();
  EXPECT_NE(options_response2.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_FALSE(options_response2.groups.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, GetOptionGroups_NoScanner) {
  GetOptionGroupsFuture future;
  document_scan_api_handler_->GetOptionGroups(extension_, "badscanner",
                                              future.GetCallback());
  const api::document_scan::GetOptionGroupsResponse& response = future.Get();

  EXPECT_EQ(response.scanner_handle, "badscanner");
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_FALSE(response.groups.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, GetOptionGroups_ValidScanner) {
  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  GetOptionGroupsFuture future;
  document_scan_api_handler_->GetOptionGroups(extension_, scanner_handle,
                                              future.GetCallback());
  const api::document_scan::GetOptionGroupsResponse& response = future.Get();

  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(response.groups.has_value());
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
  std::string scanner_id = CreateScannerIdForExtension(extension_);
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
                        .AddAPIPermission(kExtensionPermissionName)
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
  std::string scanner_id = CreateScannerIdForExtension(extension_);
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

TEST_F(DocumentScanAPIHandlerTest, SetOptions_SetBeforeOpenFails) {
  SetOptionsFuture future;
  document_scan_api_handler_->SetOptions(
      extension_, "badscanner",
      CreateTestOptionSettingList(2, api::document_scan::OptionType::kInt),
      future.GetCallback());
  const api::document_scan::SetOptionsResponse& response = future.Get();

  EXPECT_EQ(response.scanner_handle, "badscanner");
  ASSERT_EQ(response.results.size(), 2U);
  for (const auto& result : response.results) {
    EXPECT_EQ(result.result, api::document_scan::OperationResult::kInvalid);
  }
  EXPECT_FALSE(response.options.has_value());
}

// Tests the special mappings for TYPE_FIXED options.  Also indirectly tests
// getting back multiple results and an updated set of options.
TEST_F(DocumentScanAPIHandlerTest, SetOptions_FixedTypeMappings) {
  std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  OpenScannerFuture open_future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response =
      open_future.Get();
  ASSERT_TRUE(open_response.scanner_handle.has_value());
  const std::string& handle = open_response.scanner_handle.value();

  // FIXED containing no value: OK.
  // FIXED containing one int: Mapped.
  // FIXED containing int list: Mapped.
  // FIXED containing string: Wrong type.
  // FIXED containing one fixed: OK.
  // FIXED containing fixed list with decimals: OK.
  // FIXED containing fixed list without decimals: OK.
  auto settings =
      CreateTestOptionSettingList(7, api::document_scan::OptionType::kFixed);
  // settings[0] has no value.
  settings[1].value.emplace();
  settings[1].value->as_integer = 3;
  settings[2].value.emplace();
  settings[2].value->as_integers = {3, 5};
  settings[3].value.emplace();
  settings[3].value->as_string = "oops";
  settings[4].value.emplace();
  settings[4].value->as_number = 2.5;
  settings[5].value.emplace();
  settings[5].value->as_numbers = {2.5, -1.25};
  settings[6].value.emplace();
  settings[6].value->as_numbers = {2.0, -1.0};

  SetOptionsFuture future;
  document_scan_api_handler_->SetOptions(
      extension_, handle, std::move(settings), future.GetCallback());
  const api::document_scan::SetOptionsResponse& response = future.Get();
  EXPECT_EQ(response.scanner_handle, handle);
  ASSERT_EQ(response.results.size(), 7U);
  EXPECT_EQ(response.results[0].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[1].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[2].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[3].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[4].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[5].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[6].result,
            api::document_scan::OperationResult::kSuccess);

  // Verify that all supplied options are present, but assume the option value
  // conversions have already been tested by the TypeConverter unit tests.
  ASSERT_TRUE(response.options.has_value());
  for (size_t i = 1; i <= 7; i++) {
    EXPECT_TRUE(response.options->additional_properties.contains(
        base::StringPrintf("option%zu", i)));
  }
}

// Tests the special mappings for TYPE_INT options.
TEST_F(DocumentScanAPIHandlerTest, SetOptions_IntTypeMappings) {
  std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  OpenScannerFuture open_future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response =
      open_future.Get();
  ASSERT_TRUE(open_response.scanner_handle.has_value());
  const std::string& handle = open_response.scanner_handle.value();

  // INT containing no value: OK.
  // INT containing one int: OK.
  // INT containing int list: OK.
  // INT containing string: Wrong type.
  // INT containing one fixed with no fractional part: Mapped.
  // INT containing one too-large fixed with no fractional part: Wrong type.
  // INT containing one too-small fixed with no fractional part: Wrong type.
  // INT containing one fixed with non-zero fractional part: Wrong type.
  // INT containing fixed list with no fractional part: Mapped.
  // INT containing fixed list with non-zero fractional part: Wrong type.
  // INT containing fixed list with mixed fractional part: Wrong type.
  // INT containing fixed list with mixed too-small part: Wrong type.
  // INT containing fixed list with mixed too-large part: Wrong type.
  auto settings =
      CreateTestOptionSettingList(13, api::document_scan::OptionType::kInt);
  // settings[0] has no value.
  settings[1].value.emplace();
  settings[1].value->as_integer = 3;
  settings[2].value.emplace();
  settings[2].value->as_integers = {3, 5};
  settings[3].value.emplace();
  settings[3].value->as_string = "oops";
  settings[4].value.emplace();
  settings[4].value->as_number = 2.0;
  settings[5].value.emplace();
  settings[5].value->as_number = 1e300;
  settings[6].value.emplace();
  settings[6].value->as_number = -1e300;
  settings[7].value.emplace();
  settings[7].value->as_number = 2.5;
  settings[8].value.emplace();
  settings[8].value->as_numbers = {2.0, -1.0};
  settings[9].value.emplace();
  settings[9].value->as_numbers = {2.5, -1.25};
  settings[10].value.emplace();
  settings[10].value->as_numbers = {4.0, -1.25};
  settings[11].value.emplace();
  settings[11].value->as_numbers = {4.0, -1e300};
  settings[12].value.emplace();
  settings[12].value->as_numbers = {4.0, 1e300};

  SetOptionsFuture future;
  document_scan_api_handler_->SetOptions(
      extension_, handle, std::move(settings), future.GetCallback());
  const api::document_scan::SetOptionsResponse& response = future.Get();
  EXPECT_EQ(response.scanner_handle, handle);
  ASSERT_EQ(response.results.size(), 13U);
  EXPECT_EQ(response.results[0].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[1].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[2].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[3].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[4].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[5].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[6].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[7].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[8].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[9].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[10].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[11].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[12].result,
            api::document_scan::OperationResult::kWrongType);

  // Verify that all supplied options are present, but assume the option value
  // conversions have already been tested by the TypeConverter unit tests.
  ASSERT_TRUE(response.options.has_value());
  for (size_t i = 1; i <= 13; i++) {
    EXPECT_TRUE(response.options->additional_properties.contains(
        base::StringPrintf("option%zu", i)));
  }
}

TEST_F(DocumentScanAPIHandlerTest, SetOptions_BoolTypeMappings) {
  std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  OpenScannerFuture open_future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response =
      open_future.Get();
  ASSERT_TRUE(open_response.scanner_handle.has_value());
  const std::string& handle = open_response.scanner_handle.value();

  // BOOL containing no value: OK.
  // BOOL containing boolean: OK.
  // BOOL containing string: Wrong type.
  // BOOL containing int: Wrong type.
  // BOOL containing fixed: Wrong type.
  // BOOL containing int list: Wrong type.
  // BOOL containing fixed list: Wrong type.
  auto settings =
      CreateTestOptionSettingList(7, api::document_scan::OptionType::kBool);
  // settings[0] has no value.
  settings[1].value.emplace();
  settings[1].value->as_boolean = false;
  settings[2].value.emplace();
  settings[2].value->as_string = "oops";
  settings[3].value.emplace();
  settings[3].value->as_integer = 1;
  settings[4].value.emplace();
  settings[4].value->as_number = 1.5;
  settings[5].value.emplace();
  settings[5].value->as_integers = {1, 2};
  settings[6].value.emplace();
  settings[6].value->as_numbers = {1.5, 2.5};

  SetOptionsFuture future;
  document_scan_api_handler_->SetOptions(
      extension_, handle, std::move(settings), future.GetCallback());
  const api::document_scan::SetOptionsResponse& response = future.Get();
  EXPECT_EQ(response.scanner_handle, handle);
  ASSERT_EQ(response.results.size(), 7U);
  EXPECT_EQ(response.results[0].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[1].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[2].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[3].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[4].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[5].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[6].result,
            api::document_scan::OperationResult::kWrongType);

  // Verify that all supplied options are present, but assume the option value
  // conversions have already been tested by the TypeConverter unit tests.
  ASSERT_TRUE(response.options.has_value());
  for (size_t i = 1; i <= 7; i++) {
    EXPECT_TRUE(response.options->additional_properties.contains(
        base::StringPrintf("option%zu", i)));
  }
}

TEST_F(DocumentScanAPIHandlerTest, SetOptions_StringTypeMappings) {
  std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  OpenScannerFuture open_future;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response =
      open_future.Get();
  ASSERT_TRUE(open_response.scanner_handle.has_value());
  const std::string& handle = open_response.scanner_handle.value();

  // STRING containing no value: OK.
  // STRING containing string: OK.
  // STRING containing boolean: Wrong type.
  // STRING containing int: Wrong type.
  // STRING containing fixed: Wrong type.
  // STRING containing int list: Wrong type.
  // STRING containing fixed list: Wrong type.
  auto settings =
      CreateTestOptionSettingList(7, api::document_scan::OptionType::kString);
  // settings[0] has no value.
  settings[1].value.emplace();
  settings[1].value->as_string = "string";
  settings[2].value.emplace();
  settings[2].value->as_boolean = true;
  settings[3].value.emplace();
  settings[3].value->as_integer = 1;
  settings[4].value.emplace();
  settings[4].value->as_number = 1.5;
  settings[5].value.emplace();
  settings[5].value->as_integers = {1, 2};
  settings[6].value.emplace();
  settings[6].value->as_numbers = {1.5, 2.5};

  SetOptionsFuture future;
  document_scan_api_handler_->SetOptions(
      extension_, handle, std::move(settings), future.GetCallback());
  const api::document_scan::SetOptionsResponse& response = future.Get();
  EXPECT_EQ(response.scanner_handle, handle);
  ASSERT_EQ(response.results.size(), 7U);
  EXPECT_EQ(response.results[0].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[1].result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.results[2].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[3].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[4].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[5].result,
            api::document_scan::OperationResult::kWrongType);
  EXPECT_EQ(response.results[6].result,
            api::document_scan::OperationResult::kWrongType);

  // Verify that all supplied options are present, but assume the option value
  // conversions have already been tested by the TypeConverter unit tests.
  ASSERT_TRUE(response.options.has_value());
  for (size_t i = 1; i <= 7; i++) {
    EXPECT_TRUE(response.options->additional_properties.contains(
        base::StringPrintf("option%zu", i)));
  }
}

TEST_F(DocumentScanAPIHandlerTest, StartScan_PermissionDenied) {
  // There is a check for a valid scanner handle even before the permissions
  // check, so even though this test will simulate a deny permission, there has
  // to be a valid scanner.
  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  base::AutoReset<std::optional<bool>> testing_scope =
      StartScanRunner::SetStartScanConfirmationResultForTesting(false);
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result,
            api::document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_FALSE(response.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, StartScan_PermissionApproved) {
  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  base::AutoReset<std::optional<bool>> testing_scope =
      StartScanRunner::SetStartScanConfirmationResultForTesting(true);
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_TRUE(response.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, StartScan_PermissionApprovedSameHandle) {
  const std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  base::AutoReset<std::optional<bool>> testing_scope =
      StartScanRunner::SetStartScanConfirmationResultForTesting(true);
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_TRUE(response.job.has_value());

  // Set the confirmation result to false.  This shouldn't matter since this
  // scanner is already approved.
  base::AutoReset<std::optional<bool>> testing_scope2 =
      StartScanRunner::SetStartScanConfirmationResultForTesting(false);
  StartScanFuture future2;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle, std::move(options), future2.GetCallback());

  const api::document_scan::StartScanResponse& response2 = future2.Get();
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response2.scanner_handle, scanner_handle);
  EXPECT_TRUE(response2.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest,
       StartScan_PermissionApprovedSameScannerNewHandle) {
  const std::string scanner_id = CreateScannerIdForExtension(extension_);
  const std::string scanner_handle1 = OpenScannerWithId(extension_, scanner_id);
  EXPECT_FALSE(scanner_handle1.empty());

  base::AutoReset<std::optional<bool>> testing_scope =
      StartScanRunner::SetStartScanConfirmationResultForTesting(true);
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle1, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.scanner_handle, scanner_handle1);
  EXPECT_TRUE(response.job.has_value());

  // New handle pointing to the same scanner.
  const std::string scanner_handle2 = OpenScannerWithId(extension_, scanner_id);
  EXPECT_FALSE(scanner_handle2.empty());
  EXPECT_NE(scanner_handle2, scanner_handle1);

  // Set the confirmation result to false.  Starting subsequent scans will be
  // denied unless they come from a user gesture.
  base::AutoReset<std::optional<bool>> testing_scope2 =
      StartScanRunner::SetStartScanConfirmationResultForTesting(false);

  // Denied because the scan isn't initiated by a user gesture.
  StartScanFuture future2;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle2, std::move(options), future2.GetCallback());
  const api::document_scan::StartScanResponse& response2 = future2.Get();
  EXPECT_EQ(response2.scanner_handle, scanner_handle2);
  EXPECT_NE(response2.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_FALSE(response2.job.has_value());

  // Allowed because the same scanner was previously approved and this is a user
  // gesture.
  StartScanFuture future3;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      scanner_handle2, std::move(options), future3.GetCallback());
  const api::document_scan::StartScanResponse& response3 = future3.Get();
  EXPECT_EQ(response3.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response3.scanner_handle, scanner_handle2);
  EXPECT_TRUE(response3.job.has_value());

  // Allowed without a user gesture because the previous call marked the handle
  // as approved.
  StartScanFuture future4;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle2, std::move(options), future4.GetCallback());
  const api::document_scan::StartScanResponse& response4 = future4.Get();
  EXPECT_EQ(response4.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response4.scanner_handle, scanner_handle2);
  EXPECT_TRUE(response4.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, StartScan_ExtensionTrusted) {
  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  MarkExtensionTrusted(kExtensionId);
  // Confirmation will be denied, but it won't matter because the extension is
  // trusted.
  base::AutoReset<std::optional<bool>> testing_scope =
      StartScanRunner::SetStartScanConfirmationResultForTesting(false);

  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_TRUE(response.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, StartScan_HandleNotOpen) {
  // If the scanner handle has not been opened, this will fail.
  base::AutoReset<std::optional<bool>> testing_scope =
      StartScanRunner::SetStartScanConfirmationResultForTesting(true);
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false,
      "scanner-handle", std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response.scanner_handle, "scanner-handle");
  EXPECT_FALSE(response.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, StartScan_HandleNotMine) {
  MarkExtensionTrusted(kExtensionId);

  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  // Trying to start a scan using extension2 will fail since that extension is
  // not authorized to use the scanner handle opened for the first extension.
  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddAPIPermission(kExtensionPermissionName)
                        .Build();

  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension2, /*user_gesture=*/false,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_FALSE(response.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, CancelScan_InvalidJob) {
  // Since this job-handle is not valid, an error should get returned.
  CancelScanFuture future;
  document_scan_api_handler_->CancelScan(extension_, "job-handle",
                                         future.GetCallback());

  const api::document_scan::CancelScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response.job, "job-handle");
}

TEST_F(DocumentScanAPIHandlerTest, CancelScan_ValidJob) {
  // Start a scan so we can get a valid job handle, and then attempt to cancel
  // the scan using that job handle.
  std::string job_handle = StartScanForExtension(extension_);
  EXPECT_FALSE(job_handle.empty());

  CancelScanFuture cancel_future;
  document_scan_api_handler_->CancelScan(extension_, job_handle,
                                         cancel_future.GetCallback());

  const api::document_scan::CancelScanResponse& cancel_response =
      cancel_future.Get();
  EXPECT_EQ(cancel_response.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(cancel_response.job, job_handle);
}

TEST_F(DocumentScanAPIHandlerTest, CancelScan_HandleNotMine) {
  std::string job_handle = StartScanForExtension(extension_);
  EXPECT_FALSE(job_handle.empty());

  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddAPIPermission(kExtensionPermissionName)
                        .Build();

  // Trying to cancel the scan using extension2 will fail since that extension
  // is not authorized to use the job handle opened for the first extension.
  CancelScanFuture cancel_future;
  document_scan_api_handler_->CancelScan(extension2, job_handle,
                                         cancel_future.GetCallback());

  const api::document_scan::CancelScanResponse& cancel_response =
      cancel_future.Get();
  EXPECT_EQ(cancel_response.result,
            api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(cancel_response.job, job_handle);
}

TEST_F(DocumentScanAPIHandlerTest, CancelScan_GetListClears) {
  // After a `GetScannerList`, the job handles should be cleared.
  std::string job_handle = StartScanForExtension(extension_);
  EXPECT_FALSE(job_handle.empty());

  GetScannerListFuture list_future;
  document_scan_api_handler_->GetScannerList(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/false, {},
      list_future.GetCallback());
  const api::document_scan::GetScannerListResponse& list_response =
      list_future.Get();
  EXPECT_EQ(list_response.result,
            api::document_scan::OperationResult::kSuccess);

  // This cancel should fail because the GetScannerList call cleared active
  // handles.
  CancelScanFuture cancel_future;
  document_scan_api_handler_->CancelScan(extension_, job_handle,
                                         cancel_future.GetCallback());

  const api::document_scan::CancelScanResponse& cancel_response =
      cancel_future.Get();
  EXPECT_EQ(cancel_response.result,
            api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(cancel_response.job, job_handle);
}

TEST_F(DocumentScanAPIHandlerTest, ReadScanData_ReadBeforeStartFails) {
  MarkExtensionTrusted(kExtensionId);

  ReadScanDataFuture future;
  document_scan_api_handler_->ReadScanData(extension_, "job-handle",
                                           future.GetCallback());

  const api::document_scan::ReadScanDataResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response.job, "job-handle");
  EXPECT_FALSE(response.data.has_value());
  EXPECT_FALSE(response.estimated_completion.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, ReadScanData_ReadFromOpenHandleSucceeds) {
  MarkExtensionTrusted(kExtensionId);

  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());
  std::string job_handle = StartScanForScannerHandle(
      extension_, /*user_gesture=*/false, scanner_handle);
  EXPECT_FALSE(job_handle.empty());

  // First read succeeds because the job is open.
  ReadScanDataFuture read_future1;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future1.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response1 =
      read_future1.Get();

  EXPECT_EQ(read_response1.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(read_response1.job, job_handle);
  EXPECT_TRUE(read_response1.data.has_value());
  EXPECT_TRUE(read_response1.estimated_completion.has_value());

  // Second read succeeds because the job is still open.
  ReadScanDataFuture read_future2;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future2.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response2 =
      read_future2.Get();

  EXPECT_EQ(read_response2.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(read_response2.job, job_handle);
  EXPECT_TRUE(read_response2.data.has_value());
  EXPECT_TRUE(read_response2.estimated_completion.has_value());

  // Canceling the job closes the handle.
  document_scan_api_handler_->CancelScan(extension_, job_handle,
                                         base::DoNothing());

  // Third read gets a cancelled status because the job is cancelled but still
  // valid.
  ReadScanDataFuture read_future3;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future3.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response3 =
      read_future3.Get();

  EXPECT_EQ(read_response3.result,
            api::document_scan::OperationResult::kCancelled);
  EXPECT_EQ(read_response3.job, job_handle);
  EXPECT_FALSE(read_response3.data.has_value());
  EXPECT_FALSE(read_response3.estimated_completion.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, ReadScanData_ReadFromClosedScannerFails) {
  MarkExtensionTrusted(kExtensionId);

  std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());
  std::string job_handle = StartScanForScannerHandle(
      extension_, /*user_gesture=*/false, scanner_handle);
  EXPECT_FALSE(job_handle.empty());

  // First read succeeds because the job is open.
  ReadScanDataFuture read_future1;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future1.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response1 =
      read_future1.Get();
  EXPECT_EQ(read_response1.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(read_response1.job, job_handle);
  EXPECT_TRUE(read_response1.data.has_value());
  EXPECT_TRUE(read_response1.estimated_completion.has_value());

  // Closing the scanner handle also invalidates the job handle.
  document_scan_api_handler_->CloseScanner(extension_, scanner_handle,
                                           base::DoNothing());

  // Second read fails because the job is no longer valid.
  ReadScanDataFuture read_future2;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future2.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response2 =
      read_future2.Get();
  EXPECT_NE(read_response2.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(read_response2.job, job_handle);
  EXPECT_FALSE(read_response2.data.has_value());
  EXPECT_FALSE(read_response2.estimated_completion.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, ReadScanData_ReadFromReopenedScannerFails) {
  MarkExtensionTrusted(kExtensionId);

  const std::string scanner_id = CreateScannerIdForExtension(extension_);
  ASSERT_FALSE(scanner_id.empty());

  // The first open succeeds because the scanner is not open.
  OpenScannerFuture open_future1;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future1.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response1 =
      open_future1.Get();
  EXPECT_EQ(open_response1.result,
            api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(open_response1.scanner_handle.has_value());
  EXPECT_FALSE(open_response1.scanner_handle->empty());

  const std::string job_handle = StartScanForScannerHandle(
      extension_, /*user_gesture=*/false, *open_response1.scanner_handle);
  EXPECT_FALSE(job_handle.empty());

  // First read succeeds because the job is open.
  ReadScanDataFuture read_future1;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future1.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response1 =
      read_future1.Get();
  EXPECT_EQ(read_response1.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(read_response1.job, job_handle);
  EXPECT_TRUE(read_response1.data.has_value());
  EXPECT_TRUE(read_response1.estimated_completion.has_value());

  // Reopening the same scanner ID also invalidates the job handle.
  OpenScannerFuture open_future2;
  document_scan_api_handler_->OpenScanner(extension_, scanner_id,
                                          open_future2.GetCallback());
  const api::document_scan::OpenScannerResponse& open_response2 =
      open_future2.Get();
  EXPECT_EQ(open_response2.result,
            api::document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(open_response2.scanner_handle.has_value());
  EXPECT_FALSE(open_response2.scanner_handle->empty());

  // Second read fails because the job is no longer valid.
  ReadScanDataFuture read_future2;
  document_scan_api_handler_->ReadScanData(extension_, job_handle,
                                           read_future2.GetCallback());
  const api::document_scan::ReadScanDataResponse& read_response2 =
      read_future2.Get();
  EXPECT_NE(read_response2.result,
            api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(read_response2.job, job_handle);
  EXPECT_FALSE(read_response2.data.has_value());
  EXPECT_FALSE(read_response2.estimated_completion.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, UnloadExtension) {
  // Test when an extension gets unloaded, handles for that extension are closed
  // but handles for a second extension are not affected.
  MarkExtensionTrusted(kExtensionId);

  const std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddAPIPermission(kExtensionPermissionName)
                        .Build();
  MarkExtensionTrusted("extension2id");

  const std::string scanner_handle2 = OpenScannerForExtension(extension2);
  EXPECT_FALSE(scanner_handle2.empty());

  UnloadExtension();

  // Once the extension is unloaded, the state for this extension gets cleared,
  // so trying to start a scan with the scanner handle should fail.
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_FALSE(response.job.has_value());

  // However, the second extension should still work as usual.
  api::document_scan::StartScanOptions options2;
  StartScanFuture future2;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension2, /*user_gesture=*/true,
      scanner_handle2, std::move(options2), future2.GetCallback());

  const api::document_scan::StartScanResponse& response2 = future2.Get();
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kSuccess);
  EXPECT_EQ(response2.scanner_handle, scanner_handle2);
  EXPECT_TRUE(response2.job.has_value());
}

TEST_F(DocumentScanAPIHandlerTest, Shutdown) {
  // At shutdown, all handles for all extensions get closed.
  MarkExtensionTrusted(kExtensionId);

  const std::string scanner_handle = OpenScannerForExtension(extension_);
  EXPECT_FALSE(scanner_handle.empty());

  auto extension2 = ExtensionBuilder("extension2")
                        .SetID("extension2id")
                        .AddAPIPermission(kExtensionPermissionName)
                        .Build();
  MarkExtensionTrusted("extension2id");

  const std::string scanner_handle2 = OpenScannerForExtension(extension2);
  EXPECT_FALSE(scanner_handle2.empty());

  document_scan_api_handler_->Shutdown();

  // Trying to start a scan with either closed scanner handle will fail.
  api::document_scan::StartScanOptions options;
  StartScanFuture future;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension_, /*user_gesture=*/true,
      scanner_handle, std::move(options), future.GetCallback());

  const api::document_scan::StartScanResponse& response = future.Get();
  EXPECT_EQ(response.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response.scanner_handle, scanner_handle);
  EXPECT_FALSE(response.job.has_value());

  api::document_scan::StartScanOptions options2;
  StartScanFuture future2;
  document_scan_api_handler_->StartScan(
      /*native_window=*/nullptr, extension2, /*user_gesture=*/true,
      scanner_handle2, std::move(options2), future2.GetCallback());

  const api::document_scan::StartScanResponse& response2 = future2.Get();
  EXPECT_EQ(response2.result, api::document_scan::OperationResult::kInvalid);
  EXPECT_EQ(response2.scanner_handle, scanner_handle2);
  EXPECT_FALSE(response2.job.has_value());
}

}  // namespace
}  // namespace extensions
