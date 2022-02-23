// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "base/callback.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/cicerone/cicerone_client.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/dbus/seneschal/seneschal_client.h"
#include "components/drive/drive_pref_names.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace policy {

namespace {

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42", sizeof("42")) == sizeof("42");
}

}  // namespace

class DlpFilesControllerTest : public testing::Test {
 protected:
  DlpFilesControllerTest()
      : profile_(std::make_unique<TestingProfile>()),
        files_controller_(profile_.get(), &rules_manager_) {}

  DlpFilesControllerTest(const DlpFilesControllerTest&) = delete;
  DlpFilesControllerTest& operator=(const DlpFilesControllerTest&) = delete;

  ~DlpFilesControllerTest() override {}

  void SetUp() override {
    chromeos::DlpClient::InitializeFake();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  void TearDown() override {
    profile_.reset();

    chromeos::DlpClient::Shutdown();
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    DCHECK(file_system_context_);
    return file_system_context_->CreateCrackedFileSystemURL(
        kTestStorageKey, storage::kFileSystemTypeTest,
        base::FilePath::FromUTF8Unsafe(path));
  }

  void AddFilesToDlpClient() {
    ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());

    ASSERT_TRUE(temp_dir_.IsValid());
    ASSERT_TRUE(file_system_context_);

    base::MockCallback<chromeos::DlpClient::AddFileCallback> add_file_cb;

    EXPECT_CALL(add_file_cb, Run(testing::_)).Times(3);

    const base::FilePath path = temp_dir_.GetPath();

    const base::FilePath file1 = path.AppendASCII("test1.txt");
    ASSERT_TRUE(CreateDummyFile(file1));
    dlp::AddFileRequest add_file_req1;
    add_file_req1.set_file_path(file1.value());
    add_file_req1.set_source_url("example1.com");
    chromeos::DlpClient::Get()->AddFile(add_file_req1, add_file_cb.Get());

    const base::FilePath file2 = path.AppendASCII("test2.txt");
    ASSERT_TRUE(CreateDummyFile(file2));
    dlp::AddFileRequest add_file_req2;
    add_file_req2.set_file_path(file2.value());
    add_file_req2.set_source_url("example2.com");
    chromeos::DlpClient::Get()->AddFile(add_file_req2, add_file_cb.Get());

    const base::FilePath file3 = path.AppendASCII("test3.txt");
    ASSERT_TRUE(CreateDummyFile(file3));
    dlp::AddFileRequest add_file_req3;
    add_file_req3.set_file_path(file3.value());
    add_file_req3.set_source_url("example3.com");
    chromeos::DlpClient::Get()->AddFile(add_file_req3, add_file_cb.Get());

    testing::Mock::VerifyAndClearExpectations(&add_file_cb);

    file_url1_ = CreateFileSystemURL(file1.value());
    ASSERT_TRUE(file_url1_.is_valid());
    file_url2_ = CreateFileSystemURL(file2.value());
    ASSERT_TRUE(file_url2_.is_valid());
    file_url3_ = CreateFileSystemURL(file3.value());
    ASSERT_TRUE(file_url3_.is_valid());
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;

  testing::StrictMock<MockDlpRulesManager> rules_manager_;
  DlpFilesController files_controller_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  base::ScopedTempDir temp_dir_;
  storage::FileSystemURL file_url1_;
  storage::FileSystemURL file_url2_;
  storage::FileSystemURL file_url3_;
};

TEST_F(DlpFilesControllerTest, GetDisallowedTransfers_DiffFileSystem) {
  AddFilesToDlpClient();

  std::vector<storage::FileSystemURL> transferred_files(
      {file_url1_, file_url2_, file_url3_});
  std::vector<storage::FileSystemURL> disallowed_files(
      {file_url1_, file_url3_});

  EXPECT_CALL(rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RegisterFileSystem(
      chromeos::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kArchiveMountPath));
  base::ScopedClosureRunner external_mount_points_revoker(
      base::BindOnce(&storage::ExternalMountPoints::RevokeAllFileSystems,
                     base::Unretained(mount_points)));

  auto dst_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey(), "archive",
      base::FilePath("file.rar/path/in/archive"));

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  files_controller_.GetDisallowedTransfers(transferred_files, dst_url,
                                           future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(disallowed_files, future.Take());
}

TEST_F(DlpFilesControllerTest, GetDisallowedTransfers_SameFileSystem) {
  AddFilesToDlpClient();

  std::vector<storage::FileSystemURL> transferred_files(
      {file_url1_, file_url2_, file_url3_});
  std::vector<storage::FileSystemURL> disallowed_files;

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  files_controller_.GetDisallowedTransfers(transferred_files,
                                           CreateFileSystemURL("Downloads"),
                                           future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

class DlpFilesExternalDestinationTest
    : public DlpFilesControllerTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, DlpRulesManager::Component>> {
 protected:
  DlpFilesExternalDestinationTest() {
    user_manager_.AddUser(AccountId::FromUserEmailGaiaId(
        profile_->GetProfileUserName(), "12345"));
  }

  DlpFilesExternalDestinationTest(const DlpFilesExternalDestinationTest&) =
      delete;
  DlpFilesExternalDestinationTest& operator=(
      const DlpFilesExternalDestinationTest&) = delete;

  ~DlpFilesExternalDestinationTest() = default;

  void SetUp() override {
    DlpFilesControllerTest::SetUp();

    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    ASSERT_TRUE(mount_points_);

    mount_points_->RevokeAllFileSystems();

    ASSERT_TRUE(mount_points_->RegisterFileSystem(
        file_manager::util::GetAndroidFilesMountPointName(),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        base::FilePath(file_manager::util::kAndroidFilesPath)));

    ASSERT_TRUE(mount_points_->RegisterFileSystem(
        chromeos::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(),
        base::FilePath(file_manager::util::kRemovableMediaPath)));

    // Setup for Crostini.
    crostini::FakeCrostiniFeatures crostini_features;
    crostini_features.set_is_allowed_now(true);
    crostini_features.set_enabled(true);

    chromeos::DBusThreadManager::Initialize();
    chromeos::CiceroneClient::InitializeFake();
    chromeos::ConciergeClient::InitializeFake();
    chromeos::SeneschalClient::InitializeFake();

    crostini::CrostiniManager* crostini_manager =
        crostini::CrostiniManager::GetForProfile(profile_.get());
    ASSERT_TRUE(crostini_manager);
    crostini_manager->AddRunningVmForTesting(crostini::kCrostiniDefaultVmName);
    crostini_manager->AddRunningContainerForTesting(
        crostini::kCrostiniDefaultVmName,
        crostini::ContainerInfo(crostini::kCrostiniDefaultContainerName,
                                "testuser", "/home/testuser",
                                "PLACEHOLDER_IP"));
    ASSERT_TRUE(mount_points_->RegisterFileSystem(
        file_manager::util::GetCrostiniMountPointName(profile_.get()),
        storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
        file_manager::util::GetCrostiniMountDirectory(profile_.get())));

    // Setup for DriveFS.
    profile_->GetPrefs()->SetString(drive::prefs::kDriveFsProfileSalt, "a");
    drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get())
        ->SetEnabled(true);
    drive::DriveIntegrationService* integration_service =
        drive::DriveIntegrationServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(integration_service);
    base::FilePath mount_point_drive = integration_service->GetMountPointPath();
    ASSERT_TRUE(mount_points_->RegisterFileSystem(
        mount_point_drive.BaseName().value(), storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), mount_point_drive));
  }

  void TearDown() override {
    DlpFilesControllerTest::TearDown();

    chromeos::DBusThreadManager::Shutdown();
    chromeos::CiceroneClient::Shutdown();
    chromeos::ConciergeClient::Shutdown();
    chromeos::SeneschalClient::Shutdown();

    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  storage::ExternalMountPoints* mount_points_ = nullptr;
  ash::FakeChromeUserManager user_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesExternalDestinationTest,
    ::testing::Values(
        std::make_tuple("android_files",
                        "path/in/android",
                        DlpRulesManager::Component::kArc),
        std::make_tuple("removable",
                        "MyUSB/path/in/removable",
                        DlpRulesManager::Component::kUsb),
        std::make_tuple("crostini_test_termina_penguin",
                        "path/in/crostini",
                        DlpRulesManager::Component::kCrostini),
        std::make_tuple("drivefs-84675c855b63e12f384d45f033826980",
                        "root/path/in/mydrive",
                        DlpRulesManager::Component::kDrive)));

TEST_P(DlpFilesExternalDestinationTest, GetDisallowedTransfers_Component) {
  auto [mount_name, path, expected_component] = GetParam();

  AddFilesToDlpClient();

  std::vector<storage::FileSystemURL> transferred_files(
      {file_url1_, file_url2_, file_url3_});
  std::vector<storage::FileSystemURL> disallowed_files(
      {file_url1_, file_url3_});

  EXPECT_CALL(rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  files_controller_.GetDisallowedTransfers(transferred_files, dst_url,
                                           future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(disallowed_files, future.Take());
}

}  // namespace policy
