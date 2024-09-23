// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-forward.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-shared.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

namespace {

using testing::ElementsAre;

// Scanner name used for tests.
constexpr char kTestScannerName[] = "Test Scanner";
constexpr char kVirtualUSBPrinterName[] = "DavieV Virtual USB Printer (USB)";

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
    const user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id,
            /*is_affiliated=*/false, user_manager::UserType::kRegular,
            &profile_);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
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

  std::vector<std::string> GetScannerNames() {
    base::test::TestFuture<const std::vector<std::string>&> future;
    document_scan_ash().GetScannerNames(future.GetCallback());
    return future.Take();
  }

  std::tuple<mojom::ScanFailureMode, const std::optional<std::string>>
  ScanFirstPage(const std::string& scanner_name) {
    base::test::TestFuture<mojom::ScanFailureMode,
                           const std::optional<std::string>&>
        future;
    document_scan_ash().ScanFirstPage(scanner_name, future.GetCallback());

    return future.Take();
  }

  mojom::GetScannerListResponsePtr GetScannerList(
      const std::string& client_id,
      mojom::ScannerEnumFilterPtr filter) {
    base::test::TestFuture<mojom::GetScannerListResponsePtr> future;
    document_scan_ash().GetScannerList(client_id, std::move(filter),
                                       future.GetCallback());
    return future.Take();
  }

  mojom::OpenScannerResponsePtr OpenScanner(const std::string& client_id,
                                            const std::string& scanner_id) {
    base::test::TestFuture<mojom::OpenScannerResponsePtr> future;
    document_scan_ash().OpenScanner(client_id, scanner_id,
                                    future.GetCallback());
    return future.Take();
  }

  mojom::CloseScannerResponsePtr CloseScanner(
      const std::string& scanner_handle) {
    base::test::TestFuture<mojom::CloseScannerResponsePtr> future;
    document_scan_ash().CloseScanner(scanner_handle, future.GetCallback());
    return future.Take();
  }

  mojom::StartPreparedScanResponsePtr StartPreparedScan(
      const std::string& scanner_handle,
      mojom::StartScanOptionsPtr options) {
    base::test::TestFuture<mojom::StartPreparedScanResponsePtr> future;
    document_scan_ash().StartPreparedScan("scanner-handle", std::move(options),
                                          future.GetCallback());
    return future.Take();
  }

  mojom::ReadScanDataResponsePtr ReadScanData(const std::string& job_handle) {
    base::test::TestFuture<mojom::ReadScanDataResponsePtr> future;
    document_scan_ash().ReadScanData(job_handle, future.GetCallback());
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

  mojom::GetOptionGroupsResponsePtr GetOptionGroups(
      const std::string& scanner_handle) {
    base::test::TestFuture<mojom::GetOptionGroupsResponsePtr> future;
    document_scan_ash().GetOptionGroups(scanner_handle, future.GetCallback());
    return future.Take();
  }

  mojom::CancelScanResponsePtr CancelScan(const std::string& job_handle) {
    base::test::TestFuture<mojom::CancelScanResponsePtr> future;
    document_scan_ash().CancelScan(job_handle, future.GetCallback());
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

TEST_F(DocumentScanAshTest, ScanFirstPage_NoScanners) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({});
  const std::vector<std::string> scanner_names = GetScannerNames();

  EXPECT_TRUE(scanner_names.empty());
}

TEST_F(DocumentScanAshTest, ScanFirstPage_SingleScanner) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  const std::vector<std::string> scanner_names = GetScannerNames();

  EXPECT_THAT(scanner_names, ElementsAre(kTestScannerName));
}

TEST_F(DocumentScanAshTest, ScanFirstPage_MultipleScanner) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse(
      {kTestScannerName, kVirtualUSBPrinterName});
  const std::vector<std::string> scanner_names = GetScannerNames();

  EXPECT_THAT(scanner_names,
              ElementsAre(kTestScannerName, kVirtualUSBPrinterName));
}

TEST_F(DocumentScanAshTest, ScanFirstPage_InvalidScannerName) {
  const auto [failure_mode, scan_data] = ScanFirstPage("bad_scanner");

  EXPECT_EQ(failure_mode, mojom::ScanFailureMode::kDeviceBusy);
  EXPECT_FALSE(scan_data.has_value());
}

TEST_F(DocumentScanAshTest, ScanFirstPage_ScannerNoData) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  const auto [failure_mode, scan_data] = ScanFirstPage(kTestScannerName);

  EXPECT_EQ(failure_mode, mojom::ScanFailureMode::kDeviceBusy);
  EXPECT_FALSE(scan_data.has_value());
}

TEST_F(DocumentScanAshTest, ScanFirstPage_ScannerData) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  const std::vector<std::string> scan_data = {"PrettyPicture"};
  GetLorgnetteScannerManager()->SetScanResponse(scan_data);
  const auto [failure_mode, scan_data_response] =
      ScanFirstPage(kTestScannerName);

  EXPECT_EQ(failure_mode, mojom::ScanFailureMode::kNoFailure);
  ASSERT_TRUE(scan_data_response.has_value());
  EXPECT_EQ(scan_data_response.value(), "PrettyPicture");
}

TEST_F(DocumentScanAshTest, GetScannerList_BadResponse) {
  GetLorgnetteScannerManager()->SetGetScannerInfoListResponse(std::nullopt);
  auto request = mojom::ScannerEnumFilter::New();
  request->local = true;
  request->secure = true;
  const mojom::GetScannerListResponsePtr response =
      GetScannerList("client-id", std::move(request));

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
  EXPECT_EQ(response->scanners.size(), 0U);
}

TEST_F(DocumentScanAshTest, GetScannerList_GoodResponse) {
  lorgnette::ListScannersResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  lorgnette::ScannerInfo* scanner = fake_response.add_scanners();
  scanner->set_name("test:scanner");
  GetLorgnetteScannerManager()->SetGetScannerInfoListResponse(
      std::move(fake_response));
  auto request = mojom::ScannerEnumFilter::New();
  request->local = true;
  request->secure = true;
  const mojom::GetScannerListResponsePtr response =
      GetScannerList("client-id", std::move(request));

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kSuccess);
  ASSERT_EQ(response->scanners.size(), 1U);
  EXPECT_EQ(response->scanners[0]->id, "test:scanner");
}

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
  EXPECT_TRUE(base::Contains(response->options.value(), "option1-name"));
  EXPECT_TRUE(base::Contains(response->options.value(), "option2-name"));
}

TEST_F(DocumentScanAshTest, CloseScanner_BadResponse) {
  GetLorgnetteScannerManager()->SetCloseScannerResponse(std::nullopt);
  const mojom::CloseScannerResponsePtr response =
      CloseScanner("scanner-handle");

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
}

TEST_F(DocumentScanAshTest, CloseScanner_GoodResponse) {
  lorgnette::CloseScannerResponse fake_response;
  fake_response.mutable_scanner()->set_token("scanner-handle");
  fake_response.set_result(lorgnette::OPERATION_RESULT_MISSING);
  GetLorgnetteScannerManager()->SetCloseScannerResponse(
      std::move(fake_response));
  const mojom::CloseScannerResponsePtr response =
      CloseScanner("scanner-handle");

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kDeviceMissing);
}

TEST_F(DocumentScanAshTest, StartPreparedScan_BadResponse) {
  GetLorgnetteScannerManager()->SetStartPreparedScanResponse(std::nullopt);
  const mojom::StartPreparedScanResponsePtr response =
      StartPreparedScan("scanner-handle", mojom::StartScanOptions::New());

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  EXPECT_FALSE(response->job_handle.has_value());
}

TEST_F(DocumentScanAshTest, StartPreparedScan_GoodResponse) {
  lorgnette::StartPreparedScanResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  fake_response.mutable_scanner()->set_token("scanner-handle");
  fake_response.mutable_job_handle()->set_token("job-handle");
  GetLorgnetteScannerManager()->SetStartPreparedScanResponse(
      std::move(fake_response));
  auto options = mojom::StartScanOptions::New();
  options->format = "image/png";
  options->max_read_size = 32768;
  const mojom::StartPreparedScanResponsePtr response =
      StartPreparedScan("scanner-handle", std::move(options));

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  ASSERT_TRUE(response->job_handle.has_value());
  EXPECT_EQ(response->job_handle.value(), "job-handle");
}

TEST_F(DocumentScanAshTest, ReadScanData_BadResponse) {
  GetLorgnetteScannerManager()->SetReadScanDataResponse(std::nullopt);
  const mojom::ReadScanDataResponsePtr response = ReadScanData("job-handle");

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
  EXPECT_EQ(response->job_handle, "job-handle");
  EXPECT_FALSE(response->data.has_value());
}

TEST_F(DocumentScanAshTest, ReadScanData_GoodResponse) {
  lorgnette::ReadScanDataResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  fake_response.mutable_job_handle()->set_token("job-handle");
  fake_response.set_data("data");
  fake_response.set_estimated_completion(24);
  GetLorgnetteScannerManager()->SetReadScanDataResponse(
      std::move(fake_response));
  const mojom::ReadScanDataResponsePtr response = ReadScanData("job-handle");

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(response->job_handle, "job-handle");
  ASSERT_TRUE(response->data.has_value());
  EXPECT_THAT(response->data.value(), ElementsAre('d', 'a', 't', 'a'));
  ASSERT_TRUE(response->estimated_completion.has_value());
  EXPECT_EQ(response->estimated_completion.value(), 24U);
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

TEST_F(DocumentScanAshTest, GetOptionGroups_BadResponse) {
  GetLorgnetteScannerManager()->SetGetCurrentConfigResponse(std::nullopt);
  const mojom::GetOptionGroupsResponsePtr response =
      GetOptionGroups("scanner-handle");

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
  EXPECT_FALSE(response->groups.has_value());
}

TEST_F(DocumentScanAshTest, GetOptionGroups_GoodResponse) {
  lorgnette::GetCurrentConfigResponse fake_response;
  fake_response.mutable_scanner()->set_token("scanner-handle");
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);

  // Config
  lorgnette::ScannerConfig config;
  lorgnette::OptionGroup group;
  group.set_title("group-title");
  group.add_members("group-member");
  *config.add_option_groups() = std::move(group);
  *fake_response.mutable_config() = std::move(config);

  GetLorgnetteScannerManager()->SetGetCurrentConfigResponse(
      std::move(fake_response));
  const mojom::GetOptionGroupsResponsePtr response =
      GetOptionGroups("scanner-handle");

  EXPECT_EQ(response->scanner_handle, "scanner-handle");
  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kSuccess);
  EXPECT_TRUE(response->groups.has_value());
  ASSERT_EQ(response->groups.value().size(), 1U);
  EXPECT_EQ(response->groups.value()[0]->title, "group-title");
  EXPECT_THAT(response->groups.value()[0]->members,
              ElementsAre("group-member"));
}

TEST_F(DocumentScanAshTest, CancelScan_BadResponse) {
  GetLorgnetteScannerManager()->SetCancelScanResponse(std::nullopt);
  const mojom::CancelScanResponsePtr response = CancelScan("job-handle");

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kInternalError);
  EXPECT_EQ(response->job_handle, "job-handle");
}

TEST_F(DocumentScanAshTest, CancelScan_GoodResponse) {
  lorgnette::CancelScanResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  fake_response.mutable_job_handle()->set_token("job-handle");
  GetLorgnetteScannerManager()->SetCancelScanResponse(std::move(fake_response));
  const mojom::CancelScanResponsePtr response = CancelScan("job-handle");

  EXPECT_EQ(response->result, mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(response->job_handle, "job-handle");
}

}  // namespace crosapi
