// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"

#include <memory>
#include <optional>

#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/api_test_utils.h"

namespace extensions {

namespace api {

class DocumentScanScanFunctionTest : public ExtensionApiUnittest {
 public:
  DocumentScanScanFunctionTest()
      : function_(base::MakeRefCounted<DocumentScanScanFunction>()) {}
  ~DocumentScanScanFunctionTest() override = default;

  void SetUp() override {
    create_services_subscription_.emplace(
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&DocumentScanScanFunctionTest::
                                        OnWillCreateBrowserContextKeyedServices,
                                    base::Unretained(this))));
    ExtensionApiUnittest::SetUp();
    function_->set_user_gesture(true);
  }

 protected:
  void OnWillCreateBrowserContextKeyedServices(
      content::BrowserContext* context) {
    ash::LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<ash::FakeLorgnetteScannerManager>();
        }));
  }

  std::string RunFunctionAndReturnError(const std::string& args) {
    function_->set_extension(extension());
    std::string error = api_test_utils::RunFunctionAndReturnError(
        function_.get(), args, profile(), api_test_utils::FunctionMode::kNone);
    return error;
  }

  scoped_refptr<DocumentScanScanFunction> function_;
  std::optional<base::CallbackListSubscription> create_services_subscription_;
};

TEST_F(DocumentScanScanFunctionTest, UserGestureRequiredError) {
  function_->set_user_gesture(false);
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

}  // namespace api

}  // namespace extensions
