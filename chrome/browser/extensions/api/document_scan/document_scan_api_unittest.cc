// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_api.h"

#include <memory>
#include <optional>

#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/browser/extensions/extension_api_unittest.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

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

    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id = AccountId::FromUserEmail("test@test");
    fake_user_manager->AddUser(account_id);
    fake_user_manager->LoginUser(account_id);
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        fake_user_manager->GetActiveUser(), profile());
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

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
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

};

TEST_F(DocumentScanScanFunctionTest, UserGestureRequiredError) {
  function_->set_user_gesture(false);
  EXPECT_EQ("User gesture required to perform scan",
            RunFunctionAndReturnError("[{}]"));
}

TEST_F(DocumentScanScanFunctionTest, InactiveProfileError) {
  auto* fake_user_manager = static_cast<ash::FakeChromeUserManager*>(user_manager::UserManager::Get());
  const AccountId user2 = AccountId::FromUserEmail("user2@test");
  fake_user_manager->AddUser(user2);
  fake_user_manager->LoginUser(user2);
  fake_user_manager->SwitchActiveUser(user2);

  EXPECT_EQ("Document scanning is unavailable from an inactive user profile.",
            RunFunctionAndReturnError("[{}]"));
}

}  // namespace api

}  // namespace extensions
