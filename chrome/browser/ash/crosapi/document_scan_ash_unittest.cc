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

 private:
  // Must outlive `profile_`.
  content::BrowserTaskEnvironment task_environment_;

  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;

  // Must outlive `document_scan_ash_`.
  TestingProfile profile_;

  DocumentScanAsh document_scan_ash_;
};

}  // namespace crosapi
