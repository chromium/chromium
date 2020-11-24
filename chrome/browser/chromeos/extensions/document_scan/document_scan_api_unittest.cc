// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/extensions/document_scan/document_scan_api.h"
#include "chrome/browser/chromeos/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/chromeos/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api_test_utils.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/lorgnette/dbus-constants.h"

namespace extensions {

namespace api {

namespace {

// Scanner name used for tests.
constexpr char kTestScannerName[] = "Test Scanner";

// Creates a new FakeLorgnetteScannerManager for the given |context|.
std::unique_ptr<KeyedService> BuildLorgnetteScannerManager(
    content::BrowserContext* context) {
  return std::make_unique<chromeos::FakeLorgnetteScannerManager>();
}

}  // namespace

class DocumentScanScanFunctionTest : public ExtensionApiUnittest {
 public:
  DocumentScanScanFunctionTest()
      : function_(base::MakeRefCounted<DocumentScanScanFunction>()) {}
  ~DocumentScanScanFunctionTest() override {}

  void SetUp() override {
    ExtensionApiUnittest::SetUp();
    function_->set_user_gesture(true);
    chromeos::LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
        browser()->profile(),
        base::BindRepeating(&BuildLorgnetteScannerManager));
  }

  chromeos::FakeLorgnetteScannerManager* GetLorgnetteScannerManager() {
    return static_cast<chromeos::FakeLorgnetteScannerManager*>(
        chromeos::LorgnetteScannerManagerFactory::GetForBrowserContext(
            browser()->profile()));
  }

 protected:
  std::string RunFunctionAndReturnError(const std::string& args) {
    function_->set_extension(extension());
    std::string error =
        extension_function_test_utils::RunFunctionAndReturnError(
            function_.get(), args, browser(), api_test_utils::NONE);
    return error;
  }

  scoped_refptr<DocumentScanScanFunction> function_;
};

TEST_F(DocumentScanScanFunctionTest, UserGestureRequiredError) {
  function_->set_user_gesture(false);
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, NoScannersAvailableError) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({});
  EXPECT_EQ("No scanners available", RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, UnsupportedMimeTypesError) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  EXPECT_EQ("Unsupported MIME types",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"image/tiff\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, ScanImageError) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  GetLorgnetteScannerManager()->SetScanResponse(base::nullopt);
  EXPECT_EQ("Failed to scan image",
            RunFunctionAndReturnError("[{\"mimeTypes\": [\"image/png\"]}]"));
}

TEST_F(DocumentScanScanFunctionTest, Success) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  const std::vector<std::string> scan_data = {"PrettyPicture"};
  GetLorgnetteScannerManager()->SetScanResponse(scan_data);
  std::unique_ptr<base::DictionaryValue> result(RunFunctionAndReturnDictionary(
      function_.get(), "[{\"mimeTypes\": [\"image/png\"]}]"));
  ASSERT_NE(nullptr, result.get());
  document_scan::ScanResults scan_results;
  EXPECT_TRUE(document_scan::ScanResults::Populate(*result, &scan_results));
  // Verify the image data URL is the PNG image data URL prefix plus the base64
  // representation of "PrettyPicture".
  EXPECT_THAT(
      scan_results.data_urls,
      testing::ElementsAre("data:image/png;base64,UHJldHR5UGljdHVyZQ=="));
  EXPECT_EQ("image/png", scan_results.mime_type);
}

}  // namespace api

}  // namespace extensions
