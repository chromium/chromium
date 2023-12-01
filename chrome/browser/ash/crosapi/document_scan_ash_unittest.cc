// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/scanning/fake_lorgnette_scanner_manager.h"
#include "chrome/browser/ash/scanning/lorgnette_scanner_manager_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
    feature_list_.InitAndEnableFeature(ash::features::kAdvancedDocumentScanAPI);
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    ash::LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildLorgnetteScannerManager));

    const AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    const user_manager::User* user =
        fake_user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id,
            /*is_affiliated=*/false, user_manager::USER_TYPE_REGULAR,
            &profile_);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);
  }

  void TearDown() override {
    fake_user_manager_.Reset();
    feature_list_.Reset();
  }

  ash::FakeLorgnetteScannerManager* GetLorgnetteScannerManager() {
    return static_cast<ash::FakeLorgnetteScannerManager*>(
        ash::LorgnetteScannerManagerFactory::GetForBrowserContext(&profile_));
  }

  DocumentScanAsh& document_scan_ash() { return document_scan_ash_; }

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  // Must outlive `document_scan_ash_`.
  TestingProfile profile_;

  base::test::ScopedFeatureList feature_list_;

  DocumentScanAsh document_scan_ash_;
};

TEST_F(DocumentScanAshTest, ScanFirstPage_NoScanners) {
  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({});
  document_scan_ash().GetScannerNames(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& scanner_names) {
        EXPECT_TRUE(scanner_names.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ScanFirstPage_SingleScanner) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  base::RunLoop run_loop;
  document_scan_ash().GetScannerNames(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& scanner_names) {
        EXPECT_THAT(scanner_names, ElementsAre(kTestScannerName));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ScanFirstPage_MultipleScanner) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse(
      {kTestScannerName, kVirtualUSBPrinterName});
  base::RunLoop run_loop;
  document_scan_ash().GetScannerNames(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& scanner_names) {
        EXPECT_THAT(scanner_names,
                    ElementsAre(kTestScannerName, kVirtualUSBPrinterName));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ScanFirstPage_InvalidScannerName) {
  base::RunLoop run_loop;
  document_scan_ash().ScanFirstPage(
      "bad_scanner", base::BindLambdaForTesting(
                         [&](mojom::ScanFailureMode failure_mode,
                             const absl::optional<std::string>& scan_data) {
                           EXPECT_EQ(failure_mode,
                                     mojom::ScanFailureMode::kDeviceBusy);
                           EXPECT_FALSE(scan_data.has_value());
                           run_loop.Quit();
                         }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ScanFirstPage_ScannerNoData) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  base::RunLoop run_loop;
  document_scan_ash().ScanFirstPage(
      kTestScannerName, base::BindLambdaForTesting(
                            [&](mojom::ScanFailureMode failure_mode,
                                const absl::optional<std::string>& scan_data) {
                              EXPECT_EQ(failure_mode,
                                        mojom::ScanFailureMode::kDeviceBusy);
                              EXPECT_FALSE(scan_data.has_value());
                              run_loop.Quit();
                            }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ScanFirstPage_ScannerData) {
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  const std::vector<std::string> scan_data = {"PrettyPicture"};
  GetLorgnetteScannerManager()->SetScanResponse(scan_data);
  base::RunLoop run_loop;
  document_scan_ash().ScanFirstPage(
      kTestScannerName, base::BindLambdaForTesting(
                            [&](mojom::ScanFailureMode failure_mode,
                                const absl::optional<std::string>& scan_data) {
                              EXPECT_EQ(failure_mode,
                                        mojom::ScanFailureMode::kNoFailure);
                              ASSERT_TRUE(scan_data.has_value());
                              EXPECT_EQ(scan_data.value(), "PrettyPicture");
                              run_loop.Quit();
                            }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, GetScannerList_FeatureDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(ash::features::kAdvancedDocumentScanAPI);

  GetLorgnetteScannerManager()->SetGetScannerInfoListResponse(std::nullopt);
  base::RunLoop run_loop;
  auto request = mojom::ScannerEnumFilter::New();
  request->local = true;
  request->secure = true;
  document_scan_ash().GetScannerList(
      "client-id", std::move(request),
      base::BindLambdaForTesting(
          [&](mojom::GetScannerListResponsePtr response) {
            run_loop.Quit();
            EXPECT_EQ(response->result,
                      mojom::ScannerOperationResult::kUnsupported);
            EXPECT_EQ(response->scanners.size(), 0U);
          }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, GetScannerList_BadResponse) {
  GetLorgnetteScannerManager()->SetGetScannerInfoListResponse(std::nullopt);
  base::RunLoop run_loop;
  auto request = mojom::ScannerEnumFilter::New();
  request->local = true;
  request->secure = true;
  document_scan_ash().GetScannerList(
      "client-id", std::move(request),
      base::BindLambdaForTesting(
          [&](mojom::GetScannerListResponsePtr response) {
            run_loop.Quit();
            EXPECT_EQ(response->result,
                      mojom::ScannerOperationResult::kInternalError);
            EXPECT_EQ(response->scanners.size(), 0U);
          }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, GetScannerList_GoodResponse) {
  lorgnette::ListScannersResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  lorgnette::ScannerInfo* scanner = fake_response.add_scanners();
  scanner->set_name("test:scanner");
  GetLorgnetteScannerManager()->SetGetScannerInfoListResponse(
      std::move(fake_response));
  base::RunLoop run_loop;
  auto request = mojom::ScannerEnumFilter::New();
  request->local = true;
  request->secure = true;
  document_scan_ash().GetScannerList(
      "client-id", std::move(request),
      base::BindLambdaForTesting(
          [&](mojom::GetScannerListResponsePtr response) {
            run_loop.Quit();
            EXPECT_EQ(response->result,
                      mojom::ScannerOperationResult::kSuccess);
            ASSERT_EQ(response->scanners.size(), 1U);
            EXPECT_EQ(response->scanners[0]->id, "test:scanner");
          }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, OpenScanner_FeatureDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(ash::features::kAdvancedDocumentScanAPI);

  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetOpenScannerResponse(std::nullopt);
  document_scan_ash().OpenScanner(
      "client-id", "scanner-id",
      base::BindLambdaForTesting([&](mojom::OpenScannerResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->scanner_id, "scanner-id");
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kUnsupported);
        EXPECT_FALSE(response->scanner_handle.has_value());
        EXPECT_FALSE(response->options.has_value());
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, OpenScanner_BadResponse) {
  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetOpenScannerResponse(std::nullopt);
  document_scan_ash().OpenScanner(
      "client-id", "scanner-id",
      base::BindLambdaForTesting([&](mojom::OpenScannerResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->scanner_id, "scanner-id");
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kInternalError);
        EXPECT_FALSE(response->scanner_handle.has_value());
        EXPECT_FALSE(response->options.has_value());
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, OpenScanner_GoodResponse) {
  lorgnette::OpenScannerResponse fake_response;
  fake_response.mutable_scanner_id()->set_connection_string("scanner-id");
  fake_response.set_result(lorgnette::OPERATION_RESULT_DEVICE_BUSY);
  lorgnette::ScannerConfig* fake_config = fake_response.mutable_config();
  fake_config->mutable_scanner()->set_token("12345");
  (*fake_config->mutable_options())["option1-name"] = {};
  (*fake_config->mutable_options())["option2-name"] = {};
  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetOpenScannerResponse(
      std::move(fake_response));
  document_scan_ash().OpenScanner(
      "client-id", "scanner-id",
      base::BindLambdaForTesting([&](mojom::OpenScannerResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->scanner_id, "scanner-id");
        EXPECT_EQ(response->result, mojom::ScannerOperationResult::kDeviceBusy);
        ASSERT_TRUE(response->scanner_handle.has_value());
        EXPECT_EQ(response->scanner_handle.value(), "12345");
        ASSERT_TRUE(response->options.has_value());
        EXPECT_TRUE(base::Contains(response->options.value(), "option1-name"));
        EXPECT_TRUE(base::Contains(response->options.value(), "option2-name"));
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, CloseScanner_FeatureDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(ash::features::kAdvancedDocumentScanAPI);

  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetCloseScannerResponse(std::nullopt);
  document_scan_ash().CloseScanner(
      "scanner-handle",
      base::BindLambdaForTesting([&](mojom::CloseScannerResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->scanner_handle, "scanner-handle");
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kUnsupported);
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, CloseScanner_BadResponse) {
  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetCloseScannerResponse(std::nullopt);
  document_scan_ash().CloseScanner(
      "scanner-handle",
      base::BindLambdaForTesting([&](mojom::CloseScannerResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->scanner_handle, "scanner-handle");
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kInternalError);
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, CloseScanner_GoodResponse) {
  lorgnette::CloseScannerResponse fake_response;
  fake_response.mutable_scanner()->set_token("scanner-handle");
  fake_response.set_result(lorgnette::OPERATION_RESULT_MISSING);
  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetCloseScannerResponse(
      std::move(fake_response));
  document_scan_ash().CloseScanner(
      "scanner-handle",
      base::BindLambdaForTesting([&](mojom::CloseScannerResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->scanner_handle, "scanner-handle");
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kDeviceMissing);
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, StartPreparedScan_FeatureDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(ash::features::kAdvancedDocumentScanAPI);

  GetLorgnetteScannerManager()->SetStartPreparedScanResponse(std::nullopt);
  base::RunLoop run_loop;
  document_scan_ash().StartPreparedScan(
      "scanner-handle", mojom::StartScanOptions::New(),
      base::BindLambdaForTesting(
          [&](mojom::StartPreparedScanResponsePtr response) {
            run_loop.Quit();
            EXPECT_EQ(response->result,
                      mojom::ScannerOperationResult::kUnsupported);
            EXPECT_EQ(response->scanner_handle, "scanner-handle");
            EXPECT_FALSE(response->job_handle.has_value());
          }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, StartPreparedScan_BadResponse) {
  GetLorgnetteScannerManager()->SetStartPreparedScanResponse(std::nullopt);
  base::RunLoop run_loop;
  document_scan_ash().StartPreparedScan(
      "scanner-handle", mojom::StartScanOptions::New(),
      base::BindLambdaForTesting(
          [&](mojom::StartPreparedScanResponsePtr response) {
            run_loop.Quit();
            EXPECT_EQ(response->result,
                      mojom::ScannerOperationResult::kInternalError);
            EXPECT_EQ(response->scanner_handle, "scanner-handle");
            EXPECT_FALSE(response->job_handle.has_value());
          }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, StartPreparedScan_GoodResponse) {
  lorgnette::StartPreparedScanResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  fake_response.mutable_scanner()->set_token("scanner-handle");
  fake_response.mutable_job_handle()->set_token("job-handle");
  GetLorgnetteScannerManager()->SetStartPreparedScanResponse(
      std::move(fake_response));
  base::RunLoop run_loop;
  document_scan_ash().StartPreparedScan(
      "scanner-handle", mojom::StartScanOptions::New(),
      base::BindLambdaForTesting(
          [&](mojom::StartPreparedScanResponsePtr response) {
            run_loop.Quit();
            EXPECT_EQ(response->result,
                      mojom::ScannerOperationResult::kSuccess);
            EXPECT_EQ(response->scanner_handle, "scanner-handle");
            ASSERT_TRUE(response->job_handle.has_value());
            EXPECT_EQ(response->job_handle.value(), "job-handle");
          }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ReadScanData_FeatureDisabled) {
  base::test::ScopedFeatureList feature;
  feature.InitAndDisableFeature(ash::features::kAdvancedDocumentScanAPI);

  GetLorgnetteScannerManager()->SetReadScanDataResponse(std::nullopt);
  base::RunLoop run_loop;
  document_scan_ash().ReadScanData(
      "job-handle",
      base::BindLambdaForTesting([&](mojom::ReadScanDataResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kUnsupported);
        EXPECT_EQ(response->job_handle, "job-handle");
        EXPECT_FALSE(response->data.has_value());
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ReadScanData_BadResponse) {
  GetLorgnetteScannerManager()->SetReadScanDataResponse(std::nullopt);
  base::RunLoop run_loop;
  document_scan_ash().ReadScanData(
      "job-handle",
      base::BindLambdaForTesting([&](mojom::ReadScanDataResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->result,
                  mojom::ScannerOperationResult::kInternalError);
        EXPECT_EQ(response->job_handle, "job-handle");
        EXPECT_FALSE(response->data.has_value());
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, ReadScanData_GoodResponse) {
  lorgnette::ReadScanDataResponse fake_response;
  fake_response.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  fake_response.mutable_job_handle()->set_token("job-handle");
  fake_response.set_data("data");
  fake_response.set_estimated_completion(24);
  GetLorgnetteScannerManager()->SetReadScanDataResponse(
      std::move(fake_response));
  base::RunLoop run_loop;
  document_scan_ash().ReadScanData(
      "job-handle",
      base::BindLambdaForTesting([&](mojom::ReadScanDataResponsePtr response) {
        run_loop.Quit();
        EXPECT_EQ(response->result, mojom::ScannerOperationResult::kSuccess);
        EXPECT_EQ(response->job_handle, "job-handle");
        ASSERT_TRUE(response->data.has_value());
        EXPECT_THAT(response->data.value(), ElementsAre('d', 'a', 't', 'a'));
        ASSERT_TRUE(response->estimated_completion.has_value());
        EXPECT_EQ(response->estimated_completion.value(), 24U);
      }));
  run_loop.Run();
}

}  // namespace crosapi
