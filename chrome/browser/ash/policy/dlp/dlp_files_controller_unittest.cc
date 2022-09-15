// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"

#include <sys/types.h>
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
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_event.pb.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager_test_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_warn_notifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/ui/base/file_icon_util.h"
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

constexpr char kExampleUrl1[] = "https://example1.com/";
constexpr char kExampleUrl2[] = "https://example2.com/";
constexpr char kExampleUrl3[] = "https://example3.com/";
constexpr char kExampleUrl4[] = "https://example4.com/";

constexpr ino_t kInode1 = 1;
constexpr ino_t kInode2 = 2;
constexpr ino_t kInode3 = 3;
constexpr ino_t kInode4 = 4;

constexpr char kFilePath1[] = "test1.txt";
constexpr char kFilePath2[] = "test2.txt";
constexpr char kFilePath3[] = "test3.txt";
constexpr char kFilePath4[] = "test4.txt";

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42", sizeof("42")) == sizeof("42");
}

struct FilesTransferInfo {
  FilesTransferInfo(policy::DlpFilesController::FileAction files_action,
                    std::vector<ino_t> file_inodes,
                    std::vector<std::string> file_sources,
                    std::vector<std::string> file_paths)
      : files_action(files_action),
        file_inodes(file_inodes),
        file_sources(file_sources),
        file_paths(file_paths) {}

  policy::DlpFilesController::FileAction files_action;
  std::vector<ino_t> file_inodes;
  std::vector<std::string> file_sources;
  std::vector<std::string> file_paths;
};

using MockIsFilesTransferRestrictedCallback = testing::StrictMock<
    base::MockCallback<DlpFilesController::IsFilesTransferRestrictedCallback>>;
using MockCheckIfDownloadAllowedCallback = testing::StrictMock<
    base::MockCallback<DlpFilesController::CheckIfDownloadAllowedCallback>>;

}  // namespace

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
    reporting_manager_.reset();

    chromeos::DlpClient::Shutdown();
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    rules_manager_ = dlp_rules_manager.get();

    files_controller_ = std::make_unique<DlpFilesController>(*rules_manager_);

    reporting_manager_ = std::make_unique<DlpReportingManager>();
    SetReportQueueForReportingManager(reporting_manager_.get(), events,
                                      base::SequencedTaskRunnerHandle::Get());
    ON_CALL(*rules_manager_, GetReportingManager)
        .WillByDefault(::testing::Return(reporting_manager_.get()));

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

    const base::FilePath file1 = path.AppendASCII(kFilePath1);
    ASSERT_TRUE(CreateDummyFile(file1));
    ::dlp::AddFileRequest add_file_req1;
    add_file_req1.set_file_path(file1.value());
    add_file_req1.set_source_url(kExampleUrl1);
    chromeos::DlpClient::Get()->AddFile(add_file_req1, add_file_cb.Get());

    const base::FilePath file2 = path.AppendASCII(kFilePath2);
    ASSERT_TRUE(CreateDummyFile(file2));
    ::dlp::AddFileRequest add_file_req2;
    add_file_req2.set_file_path(file2.value());
    add_file_req2.set_source_url(kExampleUrl2);
    chromeos::DlpClient::Get()->AddFile(add_file_req2, add_file_cb.Get());

    const base::FilePath file3 = path.AppendASCII(kFilePath3);
    ASSERT_TRUE(CreateDummyFile(file3));
    ::dlp::AddFileRequest add_file_req3;
    add_file_req3.set_file_path(file3.value());
    add_file_req3.set_source_url(kExampleUrl3);
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
  std::unique_ptr<DlpFilesController> files_controller_;
  std::unique_ptr<DlpReportingManager> reporting_manager_;
  std::vector<DlpPolicyEvent> events;

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

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
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
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDisallowedTransfers(transferred_files, dst_url,
                                            future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(disallowed_files, future.Take());
}

TEST_F(DlpFilesControllerTest, GetDisallowedTransfers_SameFileSystem) {
  AddFilesToDlpClient();

  std::vector<storage::FileSystemURL> transferred_files(
      {file_url1_, file_url2_, file_url3_});

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDisallowedTransfers(transferred_files,
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
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDisallowedTransfers(transferred_files, dst_url,
                                            future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerTest, FilterDisallowedUploads_EmptyList) {
  AddFilesToDlpClient();

  std::vector<FileChooserFileInfoPtr> uploaded_files;

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;

  base::test::TestFuture<std::vector<FileChooserFileInfoPtr>> future;

  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(std::move(uploaded_files),
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
  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(std::move(uploaded_files),
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

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(file_url1_.path().value());
  check_files_transfer_response.add_files_paths(file_url3_.path().value());
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  base::test::TestFuture<std::vector<FileChooserFileInfoPtr>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(std::move(uploaded_files),
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
      {DlpFilesController::DlpFileMetadata(kExampleUrl1, true),
       DlpFilesController::DlpFileMetadata(kExampleUrl2, false),
       DlpFilesController::DlpFileMetadata(kExampleUrl3, true)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));

  base::test::TestFuture<std::vector<DlpFilesController::DlpFileMetadata>>
      future;
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(files_to_check, future.GetCallback());
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
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(files_to_check, future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerTest, GetDlpRestrictionDetails_Mixed) {
  DlpRulesManager::AggregatedDestinations destinations;
  destinations[DlpRulesManager::Level::kBlock].insert(kExampleUrl2);
  destinations[DlpRulesManager::Level::kAllow].insert(kExampleUrl3);

  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kUsb);
  components[DlpRulesManager::Level::kWarn].insert(
      DlpRulesManager::Component::kDrive);

  EXPECT_CALL(*rules_manager_, GetAggregatedDestinations)
      .WillOnce(testing::Return(destinations));
  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  ASSERT_TRUE(files_controller_);
  auto result = files_controller_->GetDlpRestrictionDetails(kExampleUrl1);

  ASSERT_EQ(result.size(), 3);
  std::vector<std::string> expected_urls;
  std::vector<DlpRulesManager::Component> expected_components;
  // Block:
  expected_urls.push_back(kExampleUrl2);
  expected_components.push_back(DlpRulesManager::Component::kUsb);
  EXPECT_EQ(result[0].level, DlpRulesManager::Level::kBlock);
  EXPECT_EQ(result[0].urls, expected_urls);
  EXPECT_EQ(result[0].components, expected_components);
  // Allow:
  expected_urls.clear();
  expected_urls.push_back(kExampleUrl3);
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

  ASSERT_TRUE(files_controller_);
  auto result = files_controller_->GetDlpRestrictionDetails(kExampleUrl1);
  ASSERT_EQ(result.size(), 1);
  std::vector<std::string> expected_urls;
  std::vector<DlpRulesManager::Component> expected_components;
  expected_components.push_back(DlpRulesManager::Component::kUsb);
  EXPECT_EQ(result[0].level, DlpRulesManager::Level::kBlock);
  EXPECT_EQ(result[0].urls, expected_urls);
  EXPECT_EQ(result[0].components, expected_components);
}

TEST_F(DlpFilesControllerTest, GetBlockedComponents) {
  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kArc);
  components[DlpRulesManager::Level::kBlock].insert(
      DlpRulesManager::Component::kCrostini);
  components[DlpRulesManager::Level::kWarn].insert(
      DlpRulesManager::Component::kUsb);
  components[DlpRulesManager::Level::kReport].insert(
      DlpRulesManager::Component::kDrive);

  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  ASSERT_TRUE(files_controller_);
  auto result = files_controller_->GetBlockedComponents(kExampleUrl1);
  ASSERT_EQ(result.size(), 2);
  std::vector<DlpRulesManager::Component> expected_components;
  expected_components.push_back(DlpRulesManager::Component::kArc);
  expected_components.push_back(DlpRulesManager::Component::kCrostini);
  EXPECT_EQ(result, expected_components);
}

TEST_F(DlpFilesControllerTest, CheckReportingOnIsDlpPolicyMatched) {
  AddFilesToDlpClient();

  const auto histogram_tester = base::HistogramTester();

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow));

  EXPECT_CALL(*rules_manager_, GetReportingManager).Times(testing::AnyNumber());

  ASSERT_TRUE(
      files_controller_->IsDlpPolicyMatched(DlpFilesController::FileDaemonInfo(
          kInode1, base::FilePath(kFilePath1), kExampleUrl1)));
  ASSERT_FALSE(
      files_controller_->IsDlpPolicyMatched(DlpFilesController::FileDaemonInfo(
          kInode2, base::FilePath(kFilePath2), kExampleUrl2)));
  ASSERT_FALSE(
      files_controller_->IsDlpPolicyMatched(DlpFilesController::FileDaemonInfo(
          kInode3, base::FilePath(kFilePath3), kExampleUrl3)));
  ASSERT_FALSE(
      files_controller_->IsDlpPolicyMatched(DlpFilesController::FileDaemonInfo(
          kInode4, base::FilePath(kFilePath4), kExampleUrl4)));

  ASSERT_EQ(events.size(), 3u);
  EXPECT_THAT(events[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl1, DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kBlock)));
  EXPECT_THAT(events[1], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl2, DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kReport)));
  EXPECT_THAT(events[2], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl3, DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kWarn)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetDlpHistogramPrefix() +
                                     std::string(dlp::kFileActionBlockedUMA)),
      base::BucketsAre(
          base::Bucket(DlpFilesController::FileAction::kUnknown, 1),
          base::Bucket(DlpFilesController::FileAction::kDownload, 0),
          base::Bucket(DlpFilesController::FileAction::kTransfer, 0)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetDlpHistogramPrefix() +
                                     std::string(dlp::kFileActionWarnedUMA)),
      base::BucketsAre(
          base::Bucket(DlpFilesController::FileAction::kUnknown, 1),
          base::Bucket(DlpFilesController::FileAction::kDownload, 0),
          base::Bucket(DlpFilesController::FileAction::kTransfer, 0)));
}

TEST_F(DlpFilesControllerTest, DownloadToLocalAllowed) {
  MockCheckIfDownloadAllowedCallback cb;
  EXPECT_CALL(cb, Run(/*is_allowed=*/true)).Times(1);

  files_controller_->CheckIfDownloadAllowed(
      GURL(kExampleUrl1),
      base::FilePath(
          "/home/chronos/u-0123456789abcdef/MyFiles/Downloads/img.jpg"),
      cb.Get());
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
        base::FilePath(file_manager::util::GetAndroidFilesPath())));

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

  const auto histogram_tester = base::HistogramTester();

  std::vector<DlpFilesController::FileDaemonInfo> transferred_files(
      {DlpFilesController::FileDaemonInfo(kInode1, base::FilePath(),
                                          kExampleUrl1),
       DlpFilesController::FileDaemonInfo(kInode2, base::FilePath(),
                                          kExampleUrl2),
       DlpFilesController::FileDaemonInfo(kInode3, base::FilePath(),
                                          kExampleUrl3)});
  std::vector<DlpFilesController::FileDaemonInfo> disallowed_files(
      {DlpFilesController::FileDaemonInfo(kInode1, base::FilePath(),
                                          kExampleUrl1),
       DlpFilesController::FileDaemonInfo(kInode3, base::FilePath(),
                                          kExampleUrl3)});

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(disallowed_files)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_->IsFilesTransferRestricted(
      transferred_files,
      DlpFilesController::DlpFileDestination(dst_url.path().value()),
      DlpFilesController::FileAction::kTransfer, cb.Get());

  ASSERT_EQ(events.size(), 2u);
  EXPECT_THAT(events[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl1, expected_component,
                             DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kBlock)));
  EXPECT_THAT(events[1], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl3, expected_component,
                             DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kBlock)));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetDlpHistogramPrefix() +
                                     std::string(dlp::kFileActionBlockedUMA)),
      base::BucketsAre(
          base::Bucket(DlpFilesController::FileAction::kUnknown, 0),
          base::Bucket(DlpFilesController::FileAction::kDownload, 0),
          base::Bucket(DlpFilesController::FileAction::kTransfer, 2)));
}

TEST_P(DlpFilesExternalDestinationTest, FileDownloadBlocked) {
  auto [mount_name, path, expected_component] = GetParam();

  MockCheckIfDownloadAllowedCallback cb;
  EXPECT_CALL(cb, Run(/*is_allowed=*/false)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_->CheckIfDownloadAllowed(GURL(kExampleUrl1), dst_url.path(),
                                            cb.Get());

  ASSERT_EQ(events.size(), 1);
  EXPECT_THAT(events[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl1, expected_component,
                             DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kBlock)));
}

class DlpFilesUrlDestinationTest
    : public DlpFilesControllerTest,
      public ::testing::WithParamInterface<
          std::tuple<std::vector<std::pair<ino_t, std::string>>,
                     std::string,
                     DlpRulesManager::Level,
                     std::vector<std::pair<ino_t, std::string>>>> {};
INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesUrlDestinationTest,
    ::testing::Values(
        std::make_tuple(std::vector<std::pair<ino_t, std::string>>(
                            {{kInode1, kExampleUrl1},
                             {kInode2, kExampleUrl2},
                             {kInode3, kExampleUrl3}}),
                        "https://wetransfer.com/",
                        DlpRulesManager::Level::kBlock,
                        std::vector<std::pair<ino_t, std::string>>(
                            {{kInode1, kExampleUrl1},
                             {kInode3, kExampleUrl3}})),
        std::make_tuple(std::vector<std::pair<ino_t, std::string>>(
                            {{kInode1, kExampleUrl1},
                             {kInode2, kExampleUrl2},
                             {kInode3, kExampleUrl3}}),
                        "https://drive.google.com/",
                        DlpRulesManager::Level::kAllow,
                        std::vector<std::pair<ino_t, std::string>>({}))));

TEST_P(DlpFilesUrlDestinationTest, IsFilesTransferRestricted_Url) {
  auto [transferred_data, dst, confidential_files_restriction_level,
        disallowed_data] = GetParam();

  const auto histogram_tester = base::HistogramTester();

  std::vector<DlpFilesController::FileDaemonInfo> transferred_files;
  std::vector<DlpFilesController::FileDaemonInfo> disallowed_files;
  for (const auto& [inode, src] : transferred_data)
    transferred_files.emplace_back(inode, base::FilePath(), src);
  for (const auto& [inode, src] : disallowed_data)
    disallowed_files.emplace_back(inode, base::FilePath(), src);

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(disallowed_files)).Times(1);

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination(_, _, _, _, _))
      .WillOnce(testing::Return(confidential_files_restriction_level))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(confidential_files_restriction_level));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  files_controller_->IsFilesTransferRestricted(
      transferred_files, DlpFilesController::DlpFileDestination(dst),
      DlpFilesController::FileAction::kDownload, cb.Get());

  ASSERT_EQ(events.size(), disallowed_files.size());
  for (size_t i = 0u; i < disallowed_files.size(); ++i) {
    EXPECT_THAT(events[i], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                               disallowed_files[i].source_url.spec(), dst,
                               DlpRulesManager::Restriction::kFiles,
                               confidential_files_restriction_level)));
  }

  int blocked_downloads =
      confidential_files_restriction_level == DlpRulesManager::Level::kBlock
          ? disallowed_files.size()
          : 0;

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetDlpHistogramPrefix() +
                                     std::string(dlp::kFileActionBlockedUMA)),
      base::BucketsAre(
          base::Bucket(DlpFilesController::FileAction::kDownload,
                       blocked_downloads),
          base::Bucket(DlpFilesController::FileAction::kTransfer, 0)));
}

class DlpFilesWarningDialogChoiceTest
    : public DlpFilesControllerTest,
      public ::testing::WithParamInterface<bool> {};

INSTANTIATE_TEST_SUITE_P(DlpFiles,
                         DlpFilesWarningDialogChoiceTest,
                         ::testing::Bool());

TEST_P(DlpFilesWarningDialogChoiceTest, FileDownloadWarned) {
  bool choice_result = GetParam();

  const auto histogram_tester = base::HistogramTester();

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  ASSERT_TRUE(mount_points);
  mount_points->RevokeAllFileSystems();
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      chromeos::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kRemovableMediaPath)));

  std::unique_ptr<MockDlpWarnNotifier> wrapper =
      std::make_unique<MockDlpWarnNotifier>(choice_result);
  MockDlpWarnNotifier* mock_dlp_warn_notifier = wrapper.get();
  files_controller_->SetWarnNotifierForTesting(std::move(wrapper));

  EXPECT_CALL(*mock_dlp_warn_notifier, ShowDlpWarningDialog).Times(1);

  MockCheckIfDownloadAllowedCallback cb;
  EXPECT_CALL(cb, Run(/*is_allowed=*/choice_result)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, DlpRulesManager::Component::kUsb, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  auto dst_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey(), "removable",
      base::FilePath("MyUSB/path/in/removable"));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_->CheckIfDownloadAllowed(GURL(kExampleUrl1), dst_url.path(),
                                            cb.Get());

  ASSERT_EQ(events.size(), 1 + (choice_result ? 1 : 0));
  EXPECT_THAT(events[0], IsDlpPolicyEvent(CreateDlpPolicyEvent(
                             kExampleUrl1, DlpRulesManager::Component::kUsb,
                             DlpRulesManager::Restriction::kFiles,
                             DlpRulesManager::Level::kWarn)));
  if (choice_result) {
    EXPECT_THAT(events[1],
                IsDlpPolicyEvent(CreateDlpPolicyWarningProceededEvent(
                    kExampleUrl1, DlpRulesManager::Component::kUsb,
                    DlpRulesManager::Restriction::kFiles)));
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(GetDlpHistogramPrefix() +
                                     std::string(dlp::kFileActionWarnedUMA)),
      base::BucketsAre(
          base::Bucket(DlpFilesController::FileAction::kDownload, 1),
          base::Bucket(DlpFilesController::FileAction::kTransfer, 0)));

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  GetDlpHistogramPrefix() +
                  std::string(dlp::kFileActionWarnProceededUMA)),
              base::BucketsAre(
                  base::Bucket(DlpFilesController::FileAction::kDownload,
                               choice_result),
                  base::Bucket(DlpFilesController::FileAction::kTransfer, 0)));

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

class DlpFilesWarningDialogContentTest
    : public DlpFilesControllerTest,
      public ::testing::WithParamInterface<FilesTransferInfo> {};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesWarningDialogContentTest,
    ::testing::Values(
        FilesTransferInfo(policy::DlpFilesController::FileAction::kDownload,
                          std::vector<ino_t>({kInode1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::DlpFilesController::FileAction::kTransfer,
                          std::vector<ino_t>({kInode1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::DlpFilesController::FileAction::kTransfer,
                          std::vector<ino_t>({kInode1, kInode2}),
                          std::vector<std::string>({kExampleUrl1,
                                                    kExampleUrl2}),
                          std::vector<std::string>({kFilePath1, kFilePath2}))));

TEST_P(DlpFilesWarningDialogContentTest,
       IsFilesTransferRestricted_WarningDialogContent) {
  auto transfer_info = GetParam();
  std::vector<DlpFilesController::FileDaemonInfo> warned_files;
  for (int i = 0; i < transfer_info.file_sources.size(); ++i) {
    warned_files.emplace_back(transfer_info.file_inodes[i],
                              base::FilePath(transfer_info.file_paths[i]),
                              transfer_info.file_sources[i]);
  }
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  ASSERT_TRUE(mount_points);
  mount_points->RevokeAllFileSystems();
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      chromeos::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kRemovableMediaPath)));
  AddFilesToDlpClient();

  std::unique_ptr<MockDlpWarnNotifier> wrapper =
      std::make_unique<MockDlpWarnNotifier>(false);
  MockDlpWarnNotifier* mock_dlp_warn_notifier = wrapper.get();
  files_controller_->SetWarnNotifierForTesting(std::move(wrapper));
  DlpConfidentialContents expected_contents;

  if (transfer_info.files_action != DlpFilesController::FileAction::kDownload) {
    for (int i = 0; i < transfer_info.file_sources.size(); ++i) {
      expected_contents.Add(
          chromeos::GetIconForPath(
              base::FilePath(std::string(transfer_info.file_paths[i])),
              /*dark_background=*/false),
          base::UTF8ToUTF16(transfer_info.file_paths[i]),
          GURL(transfer_info.file_sources[i]));
    }
  }
  DlpWarnDialog::DlpWarnDialogOptions expected_dialog_options(
      DlpWarnDialog::Restriction::kFiles, expected_contents,
      DlpRulesManager::Component::kUsb, /*destination_pattern=*/"",
      transfer_info.files_action);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, DlpRulesManager::Component::kUsb, _, _))
      .WillRepeatedly(testing::Return(DlpRulesManager::Level::kWarn));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  EXPECT_CALL(*mock_dlp_warn_notifier,
              ShowDlpWarningDialog(_, expected_dialog_options))
      .Times(1);

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(warned_files)).Times(1);

  auto dst_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey(), "removable",
      base::FilePath("MyUSB/path/in/removable"));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_->IsFilesTransferRestricted(
      warned_files,
      DlpFilesController::DlpFileDestination(dst_url.path().value()),
      transfer_info.files_action, cb.Get());

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

}  // namespace policy
