// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/bind.h"
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
    ash::LorgnetteScannerManagerFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&BuildLorgnetteScannerManager));
  }

  std::unique_ptr<ash::FakeChromeUserManager> CreateLoggedInUser() {
    auto fake_user_manager = std::make_unique<ash::FakeChromeUserManager>();
    const AccountId account_id =
        AccountId::FromUserEmail(profile_.GetProfileUserName());
    const user_manager::User* user =
        fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
            account_id,
            /*is_affiliated=*/false, user_manager::USER_TYPE_REGULAR,
            &profile_);
    fake_user_manager->UserLoggedIn(account_id, user->username_hash(),
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    fake_user_manager->SimulateUserProfileLoad(account_id);
    return fake_user_manager;
  }

  ash::FakeLorgnetteScannerManager* GetLorgnetteScannerManager() {
    return static_cast<ash::FakeLorgnetteScannerManager*>(
        ash::LorgnetteScannerManagerFactory::GetForBrowserContext(&profile_));
  }

  DocumentScanAsh& document_scan_ash() { return document_scan_ash_; }

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;

  // Must outlive `document_scan_ash_`.
  TestingProfile profile_;

  DocumentScanAsh document_scan_ash_;
};

TEST_F(DocumentScanAshTest, NoScanners) {
  user_manager::ScopedUserManager scoped_user_manager(CreateLoggedInUser());

  base::RunLoop run_loop;
  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({});
  document_scan_ash().GetScannerNames(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& scanner_names) {
        EXPECT_TRUE(scanner_names.empty());
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, SingleScanner) {
  user_manager::ScopedUserManager scoped_user_manager(CreateLoggedInUser());

  GetLorgnetteScannerManager()->SetGetScannerNamesResponse({kTestScannerName});
  base::RunLoop run_loop;
  document_scan_ash().GetScannerNames(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& scanner_names) {
        EXPECT_THAT(scanner_names, testing::ElementsAre(kTestScannerName));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, MultipleScanner) {
  user_manager::ScopedUserManager scoped_user_manager(CreateLoggedInUser());

  GetLorgnetteScannerManager()->SetGetScannerNamesResponse(
      {kTestScannerName, kVirtualUSBPrinterName});
  base::RunLoop run_loop;
  document_scan_ash().GetScannerNames(base::BindLambdaForTesting(
      [&](const std::vector<std::string>& scanner_names) {
        EXPECT_THAT(
            scanner_names,
            testing::ElementsAre(kTestScannerName, kVirtualUSBPrinterName));
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(DocumentScanAshTest, InvalidScannerName) {
  user_manager::ScopedUserManager scoped_user_manager(CreateLoggedInUser());

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

TEST_F(DocumentScanAshTest, ScannerNoData) {
  user_manager::ScopedUserManager scoped_user_manager(CreateLoggedInUser());

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

TEST_F(DocumentScanAshTest, ScannerData) {
  user_manager::ScopedUserManager scoped_user_manager(CreateLoggedInUser());

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

}  // namespace crosapi
