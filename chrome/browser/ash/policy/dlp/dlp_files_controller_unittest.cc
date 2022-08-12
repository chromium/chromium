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
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/drive/drive_pref_names.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/choosers/file_chooser.mojom.h"

using blink::mojom::FileChooserFileInfo;
using blink::mojom::FileChooserFileInfoPtr;
using blink::mojom::FileSystemFileInfo;
using blink::mojom::NativeFileInfo;
using testing::_;
using testing::Mock;

namespace policy {

namespace {

constexpr char kEmailId[] = "test@example.com";
constexpr char kGaiaId[] = "12345";

constexpr char kExample1[] = "https://example1.com";
constexpr char kExample2[] = "https://example2.com";
constexpr char kExample3[] = "https://example3.com";

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42", sizeof("42")) == sizeof("42");
}

}  // namespace

using MockIsFilesTransferRestrictedCallback = testing::StrictMock<
    base::MockCallback<DlpFilesController::IsFilesTransferRestrictedCallback>>;

class DlpFilesControllerTest : public testing::Test {
 public:
  DlpFilesControllerTest(const DlpFilesControllerTest&) = delete;
  DlpFilesControllerTest& operator=(const DlpFilesControllerTest&) = delete;

 protected:
  DlpFilesControllerTest()
      : profile_(std::make_unique<TestingProfile>()),
        user_manager_(new ash::FakeChromeUserManager()),
        scoped_user_manager_(std::make_unique<user_manager::ScopedUserManager>(
            base::WrapUnique(user_manager_))) {}

  ~DlpFilesControllerTest() override = default;

  void SetUp() override {
    AccountId account_id = AccountId::FromUserEmailGaiaId(kEmailId, kGaiaId);
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::USER_TYPE_REGULAR, profile_.get());
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    user_manager_->SimulateUserProfileLoad(account_id);

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_.get(),
        base::BindRepeating(&DlpFilesControllerTest::SetDlpRulesManager,
                            base::Unretained(this)));
    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_TRUE(rules_manager_);

    chromeos::DlpClient::InitializeFake();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());
  }

  void TearDown() override {
    scoped_user_manager_.reset();
    profile_.reset();

    chromeos::DlpClient::Shutdown();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    rules_manager_ = dlp_rules_manager.get();
    return dlp_rules_manager;
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
    add_file_req1.set_source_url(kExample1);
    chromeos::DlpClient::Get()->AddFile(add_file_req1, add_file_cb.Get());

    const base::FilePath file2 = path.AppendASCII("test2.txt");
    ASSERT_TRUE(CreateDummyFile(file2));
    dlp::AddFileRequest add_file_req2;
    add_file_req2.set_file_path(file2.value());
    add_file_req2.set_source_url(kExample2);
    chromeos::DlpClient::Get()->AddFile(add_file_req2, add_file_cb.Get());

    const base::FilePath file3 = path.AppendASCII("test3.txt");
    ASSERT_TRUE(CreateDummyFile(file3));
    dlp::AddFileRequest add_file_req3;
    add_file_req3.set_file_path(file3.value());
    add_file_req3.set_source_url(kExample3);
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
  ash::FakeChromeUserManager* user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;

  MockDlpRulesManager* rules_manager_ = nullptr;
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

  dlp::CheckFilesTransferResponse check_files_transfer_response;
  for (const auto& file : disallowed_files) {
    check_files_transfer_response.add_files_paths(file.path().value());
  }
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

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

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  files_controller_.GetDisallowedTransfers(transferred_files,
                                           CreateFileSystemURL("Downloads"),
                                           future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerTest, GetDisallowedTransfers_ClientNotRunning) {
  AddFilesToDlpClient();

  std::vector<storage::FileSystemURL> transferred_files(
      {file_url1_, file_url2_, file_url3_});

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

  chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(false);
  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  files_controller_.GetDisallowedTransfers(transferred_files, dst_url,
                                           future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerTest, FilterDisallowedUploads_EmptyList) {
  AddFilesToDlpClient();

  std::vector<FileChooserFileInfoPtr> uploaded_files;

  dlp::CheckFilesTransferResponse check_files_transfer_response;

  base::test::TestFuture<std::vector<FileChooserFileInfoPtr>> future;
  files_controller_.FilterDisallowedUploads(std::move(uploaded_files),
                                            GURL("https://example.com"),
                                            future.GetCallback());

  std::vector<FileChooserFileInfoPtr> filtered_uploads;

  ASSERT_EQ(0u, future.Get().size());
  EXPECT_EQ(filtered_uploads, future.Take());
}

TEST_F(DlpFilesControllerTest, FilterDisallowedUploads_NonNativeFiles) {
  AddFilesToDlpClient();

  std::vector<FileChooserFileInfoPtr> uploaded_files;
  uploaded_files.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));
  uploaded_files.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));
  uploaded_files.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));

  base::test::TestFuture<std::vector<FileChooserFileInfoPtr>> future;
  files_controller_.FilterDisallowedUploads(std::move(uploaded_files),
                                            GURL("https://example.com"),
                                            future.GetCallback());

  std::vector<FileChooserFileInfoPtr> filtered_uploads;
  filtered_uploads.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));
  filtered_uploads.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));
  filtered_uploads.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));

  ASSERT_EQ(3u, future.Get().size());
  EXPECT_EQ(filtered_uploads, future.Take());
}

TEST_F(DlpFilesControllerTest, FilterDisallowedUploads_MixedFiles) {
  AddFilesToDlpClient();

  std::vector<FileChooserFileInfoPtr> uploaded_files;
  uploaded_files.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));
  uploaded_files.push_back(FileChooserFileInfo::NewNativeFile(
      NativeFileInfo::New(file_url1_.path(), std::u16string())));
  uploaded_files.push_back(FileChooserFileInfo::NewNativeFile(
      NativeFileInfo::New(file_url2_.path(), std::u16string())));
  uploaded_files.push_back(FileChooserFileInfo::NewNativeFile(
      NativeFileInfo::New(file_url3_.path(), std::u16string())));
  uploaded_files.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));

  dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(file_url1_.path().value());
  check_files_transfer_response.add_files_paths(file_url3_.path().value());
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  base::test::TestFuture<std::vector<FileChooserFileInfoPtr>> future;
  files_controller_.FilterDisallowedUploads(std::move(uploaded_files),
                                            GURL("https://example.com"),
                                            future.GetCallback());

  std::vector<FileChooserFileInfoPtr> filtered_uploads;
  filtered_uploads.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));
  filtered_uploads.push_back(FileChooserFileInfo::NewNativeFile(
      NativeFileInfo::New(file_url2_.path(), std::u16string())));
  filtered_uploads.push_back(
      FileChooserFileInfo::NewFileSystem(FileSystemFileInfo::New()));

  ASSERT_EQ(3u, future.Get().size());
  EXPECT_EQ(filtered_uploads, future.Take());
}

TEST_F(DlpFilesControllerTest, GetDlpMetadata) {
  AddFilesToDlpClient();

  std::vector<storage::FileSystemURL> files_to_check(
      {file_url1_, file_url2_, file_url3_});
  std::vector<DlpFilesController::DlpFileMetadata> dlp_metadata(
      {DlpFilesController::DlpFileMetadata(kExample1, true),
       DlpFilesController::DlpFileMetadata(kExample2, false),
       DlpFilesController::DlpFileMetadata(kExample3, true)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));

  base::test::TestFuture<std::vector<DlpFilesController::DlpFileMetadata>>
      future;
  files_controller_.GetDlpMetadata(files_to_check, future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerTest, GetDlpMetadata_FileNotAvailable) {
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());

  std::vector<storage::FileSystemURL> files_to_check({file_url1_});
  std::vector<DlpFilesController::DlpFileMetadata> dlp_metadata(
      {DlpFilesController::DlpFileMetadata("", false)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule).Times(0);

  base::test::TestFuture<std::vector<DlpFilesController::DlpFileMetadata>>
      future;
  files_controller_.GetDlpMetadata(files_to_check, future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerTest, GetDlpRestrictionDetails_Mixed) {
  DlpRulesManager::AggregatedDestinations destinations;
  destinations[DlpRulesManager::Level::kBlock].insert(kExample2);
  destinations[DlpRulesManager::Level::kAllow].insert(kExample3);

  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kUsb);
  components[DlpRulesManager::Level::kWarn].insert(
      DlpRulesManager::Component::kDrive);

  EXPECT_CALL(*rules_manager_, GetAggregatedDestinations)
      .WillOnce(testing::Return(destinations));
  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  auto result = files_controller_.GetDlpRestrictionDetails(kExample1);

  ASSERT_EQ(result.size(), 3);
  std::vector<std::string> expected_urls;
  std::vector<DlpRulesManager::Component> expected_components;
  // Block:
  expected_urls.push_back(kExample2);
  expected_components.push_back(DlpRulesManager::Component::kUsb);
  EXPECT_EQ(result[0].level, DlpRulesManager::Level::kBlock);
  EXPECT_EQ(result[0].urls, expected_urls);
  EXPECT_EQ(result[0].components, expected_components);
  // Allow:
  expected_urls.clear();
  expected_urls.push_back(kExample3);
  expected_components.clear();
  EXPECT_EQ(result[1].level, DlpRulesManager::Level::kAllow);
  EXPECT_EQ(result[1].urls, expected_urls);
  EXPECT_EQ(result[1].components, expected_components);
  // Warn:
  expected_urls.clear();
  expected_components.clear();
  expected_components.push_back(DlpRulesManager::Component::kDrive);
  EXPECT_EQ(result[2].level, DlpRulesManager::Level::kWarn);
  EXPECT_EQ(result[2].urls, expected_urls);
  EXPECT_EQ(result[2].components, expected_components);
}

TEST_F(DlpFilesControllerTest, GetDlpRestrictionDetails_Components) {
  DlpRulesManager::AggregatedDestinations destinations;
  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kUsb);

  EXPECT_CALL(*rules_manager_, GetAggregatedDestinations)
      .WillOnce(testing::Return(destinations));
  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  auto result = files_controller_.GetDlpRestrictionDetails(kExample1);

  ASSERT_EQ(result.size(), 1);
  std::vector<std::string> expected_urls;
  std::vector<DlpRulesManager::Component> expected_components;
  expected_components.push_back(DlpRulesManager::Component::kUsb);
  EXPECT_EQ(result[0].level, DlpRulesManager::Level::kBlock);
  EXPECT_EQ(result[0].urls, expected_urls);
  EXPECT_EQ(result[0].components, expected_components);
}

class DlpFilesExternalDestinationTest
    : public DlpFilesControllerTest,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, DlpRulesManager::Component>> {
 public:
  DlpFilesExternalDestinationTest(const DlpFilesExternalDestinationTest&) =
      delete;
  DlpFilesExternalDestinationTest& operator=(
      const DlpFilesExternalDestinationTest&) = delete;

 protected:
  DlpFilesExternalDestinationTest() = default;

  ~DlpFilesExternalDestinationTest() override = default;

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

    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();

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

    ash::ChunneldClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::SeneschalClient::Shutdown();

    storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  storage::ExternalMountPoints* mount_points_ = nullptr;
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

TEST_P(DlpFilesExternalDestinationTest, IsFilesTransferRestricted_Component) {
  auto [mount_name, path, expected_component] = GetParam();

  std::vector<GURL> files_sources(
      {GURL(kExample1), GURL(kExample2), GURL(kExample3)});
  std::vector<GURL> disallowed_sources({GURL(kExample1), GURL(kExample3)});

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(disallowed_sources)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_.IsFilesTransferRestricted(profile_.get(), files_sources,
                                              dst_url.path().value(), cb.Get());
}

}  // namespace policy
