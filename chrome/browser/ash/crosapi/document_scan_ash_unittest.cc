// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-shared.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

using testing::ElementsAre;

// Creates a new FakeLorgnetteScannerManager for the given `context`.
std::unique_ptr<KeyedService> BuildLorgnetteScannerManager(
    content::BrowserContext* context) {
  return std::make_unique<ash::FakeLorgnetteScannerManager>();
}

}  // namespace

class DocumentScanAshTest : public testing::Test {
 public:
  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    ash::LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildLorgnetteScannerManager));

    const AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
        account_id,
        /*is_affiliated=*/false, user_manager::UserType::kRegular, &profile_);
    fake_user_manager_->UserLoggedIn(
        account_id, user_manager::TestHelper::GetFakeUsernameHash(account_id));
    fake_user_manager_->SimulateUserProfileLoad(account_id);
  }

  void TearDown() override {
    fake_user_manager_.Reset();
  }

  ash::FakeLorgnetteScannerManager* GetLorgnetteScannerManager() {
    return static_cast<ash::FakeLorgnetteScannerManager*>(
        ash::LorgnetteScannerManagerFactory::GetForBrowserContext(&profile_));
  }

  DocumentScanAsh& document_scan_ash() { return document_scan_ash_; }

  mojom::OpenScannerResponsePtr OpenScanner(const std::string& client_id,
                                            const std::string& scanner_id) {
    base::test::TestFuture<mojom::OpenScannerResponsePtr> future;
    document_scan_ash().OpenScanner(client_id, scanner_id,
                                    future.GetCallback());
    return future.Take();
  }

  mojom::SetOptionsResponsePtr SetOptions(
      const std::string& scanner_handle,
      std::vector<mojom::OptionSettingPtr> options) {
    base::test::TestFuture<mojom::SetOptionsResponsePtr> future;
    document_scan_ash().SetOptions(scanner_handle, std::move(options),
                                   future.GetCallback());
    return future.Take();
  }


 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  // Must outlive `document_scan_ash_`.
  TestingProfile profile_;

  DocumentScanAsh document_scan_ash_;
};

TEST_F(DocumentScanAshTest, OpenScanner_BadResponse) {
  GetLorgnetteScannerManager()->SetOpenScannerResponse(std::nullopt);
  const mojom::OpenScannerResponsePtr response =
      OpenScanner("client-id", "scanner-id");

  EXPECT_EQ(response->scanner_id, "scanner-id");
  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
  EXPECT_FALSE(response->scanner_handle.has_value());
  EXPECT_FALSE(response->options.has_value());
}

TEST_F(DocumentScanAshTest, OpenScanner_GoodResponse) {
  lorgnette::OpenScannerResponse fake_response;
  fake_response.mutable_scanner_id()->set_connection_string("scanner-id");
  fake_response.set_result(lorgnette::OPERATION_RESULT_DEVICE_BUSY);
  lorgnette::ScannerConfig* fake_config = fake_response.mutable_config();
  fake_config->mutable_scanner()->set_token("12345");
  (*fake_config->mutable_options())["option1-name"] = {};
  (*fake_config->mutable_options())["option2-name"] = {};
  GetLorgnetteScannerManager()->SetOpenScannerResponse(
      std::move(fake_response));
  const mojom::OpenScannerResponsePtr response =
      OpenScanner("client-id", "scanner-id");

  EXPECT_EQ(response->scanner_id, "scanner-id");
  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kDeviceBusy);
  ASSERT_TRUE(response->scanner_handle.has_value());
  EXPECT_EQ(response->scanner_handle.value(), "12345");
  ASSERT_TRUE(response->options.has_value());
  EXPECT_TRUE(response->options.value().contains("option1-name"));
  EXPECT_TRUE(response->options.value().contains("option2-name"));
}

TEST_F(DocumentScanAshTest, SetOptions_BadResponse) {
  auto option = mojom::OptionSetting::New();
  option->name = "option-name";
  std::vector<mojom::OptionSettingPtr> options;
  options.emplace_back(std::move(option));

  GetLorgnetteScannerManager()->SetSetOptionsResponse(std::nullopt);
  const mojom::SetOptionsResponsePtr response =
      SetOptions("scanner-handle", std::move(options));

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  EXPECT_FALSE(response->options.has_value());
  ASSERT_EQ(response->results.size(), 1U);
  EXPECT_EQ(response->results[0]->name, "option-name");
  EXPECT_EQ(response->results[0]->result,
            mojom::ScannerOperationResult::kInternalError);
}

TEST_F(DocumentScanAshTest, SetOptions_BadValue) {
  // Insert an option whose value type is different than the option type.
  auto option = mojom::OptionSetting::New();
  option->name = "bad-option";
  option->type = mojom::OptionType::kString;
  option->value = crosapi::mojom::OptionValue::NewBoolValue(false);
  std::vector<mojom::OptionSettingPtr> options;
  options.emplace_back(std::move(option));

  // Don't add any results to our fake response.  The result in our response
  // should come from our DocumentScanAsh object itself, not from the backend.
  lorgnette::SetOptionsResponse fake_response;
  fake_response.mutable_scanner()->set_token("scanner-handle");

  GetLorgnetteScannerManager()->SetSetOptionsResponse(std::move(fake_response));
  const mojom::SetOptionsResponsePtr response =
      SetOptions("scanner-handle", std::move(options));

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  ASSERT_EQ(response->results.size(), 1U);
  EXPECT_EQ(response->results[0]->name, "bad-option");
  EXPECT_EQ(response->results[0]->result,
            mojom::ScannerOperationResult::kWrongType);
}

TEST_F(DocumentScanAshTest, SetOptions_GoodResponse) {
  // The options we put in here don't really matter since we are setting the
  // response manually in the fake lorgnette scanner manager.
  std::vector<mojom::OptionSettingPtr> options;

  lorgnette::SetOptionsResponse fake_response;
  fake_response.mutable_scanner()->set_token("scanner-handle");
  (*fake_response.mutable_results())["option-name"] =
      lorgnette::OPERATION_RESULT_SUCCESS;

  // Config
  lorgnette::ScannerConfig config;
  lorgnette::ScannerOption scanner_option;
  scanner_option.set_name("scanner-option");
  (*config.mutable_options())["config-option"] = std::move(scanner_option);
  *fake_response.mutable_config() = std::move(config);

  GetLorgnetteScannerManager()->SetSetOptionsResponse(std::move(fake_response));
  const mojom::SetOptionsResponsePtr response =
      SetOptions("scanner-handle", std::move(options));

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  ASSERT_EQ(response->results.size(), 1U);
  EXPECT_EQ(response->results[0]->name, "option-name");
  EXPECT_EQ(response->results[0]->result,
            mojom::ScannerOperationResult::kSuccess);
  ASSERT_TRUE(response->options.has_value());
  const auto& it = response->options.value().find("config-option");
  ASSERT_TRUE(it != response->options.value().end());
  EXPECT_EQ(it->second->name, "scanner-option");
}

}  // namespace crosapi
