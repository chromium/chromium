// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/scanning/scanning_paths_provider_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/drive/drive_integration_service.h"
#include "chrome/browser/chromeos/drive/drivefs_test_support.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/net/network_portal_detector_test_impl.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/account_id/account_id.h"
#include "components/drive/drive_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

constexpr char kRoot[] = "root";

}  // namespace

class ScanningPathsProviderImplTest : public testing::Test {
 public:
  ScanningPathsProviderImplTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ScanningPathsProviderImplTest() override = default;

  void SetUp() override {
    chromeos::LoginState::Initialize();
    chromeos::DBusThreadManager::Initialize();
    if (!network_portal_detector::IsInitialized()) {
      network_portal_detector_ = new NetworkPortalDetectorTestImpl();
      network_portal_detector::InitializeForTesting(network_portal_detector_);
    }

    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("profile1@gmail.com");
    profile_->GetPrefs()->SetBoolean(drive::prefs::kDriveFsPinnedMigrated,
                                     true);
    user_manager_.AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(web_contents_.get());

    create_drive_integration_service_ = base::Bind(
        &ScanningPathsProviderImplTest::CreateDriveIntegrationService,
        base::Unretained(this));
    service_factory_for_test_ = std::make_unique<
        drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>(
        &create_drive_integration_service_);
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(profile_);
    integration_service->OnMounted(drive_mount_point_);
    drive_mount_point_ = integration_service->GetMountPointPath();
  }

  void TearDown() override {
    chromeos::LoginState::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  drive::DriveIntegrationService* CreateDriveIntegrationService(
      Profile* profile) {
    return new drive::DriveIntegrationService(
        profile, std::string(), profile->GetPath().Append("drivefs"),
        fake_drivefs_helper_->CreateFakeDriveFsListenerFactory());
  }

  TestingProfile* profile_;
  ScanningPathsProviderImpl scanning_paths_provider_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  base::FilePath drive_mount_point_;

 private:
  chromeos::FakeChromeUserManager user_manager_;
  NetworkPortalDetectorTestImpl* network_portal_detector_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<drive::DriveIntegrationServiceFactory::ScopedFactoryForTest>
      service_factory_for_test_;
  drive::DriveIntegrationServiceFactory::FactoryCallback
      create_drive_integration_service_;
  std::unique_ptr<drive::FakeDriveFsHelper> fake_drivefs_helper_;
};

// Validates that sending the Google Drive root filepath returns 'My Drive'.
TEST_F(ScanningPathsProviderImplTest, MyDrivePath) {
  EXPECT_EQ("directory", scanning_paths_provider_.GetBaseNameFromPath(
                             web_ui_.get(), base::FilePath("/test/directory")));
  EXPECT_EQ("Sub Folder",
            scanning_paths_provider_.GetBaseNameFromPath(
                web_ui_.get(),
                drive_mount_point_.Append(kRoot).Append("Sub Folder")));
  EXPECT_EQ("My Drive", scanning_paths_provider_.GetBaseNameFromPath(
                            web_ui_.get(), drive_mount_point_.Append(kRoot)));
}

// Validates that sending the MyFiles filepath returns 'My files'.
TEST_F(ScanningPathsProviderImplTest, MyFilesPath) {
  base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);

  EXPECT_EQ("directory", scanning_paths_provider_.GetBaseNameFromPath(
                             web_ui_.get(), base::FilePath("/test/directory")));
  EXPECT_EQ("Sub Folder",
            scanning_paths_provider_.GetBaseNameFromPath(
                web_ui_.get(), my_files_path.Append("Sub Folder")));
  EXPECT_EQ("My files", scanning_paths_provider_.GetBaseNameFromPath(
                            web_ui_.get(), my_files_path));
}

}  // namespace chromeos.
