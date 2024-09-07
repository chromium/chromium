// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/repeating_test_future.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_test.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/ash/policy/dlp/test/dlp_files_test_with_mounts.h"
#include "chrome/browser/ash/policy/dlp/test/files_policy_notification_manager_test_utils.h"
#include "chrome/browser/ash/policy/dlp/test/mock_files_policy_notification_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chrome/browser/enterprise/data_controls/dlp_reporting_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "components/drive/drive_pref_names.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "components/enterprise/data_controls/core/browser/dlp_histogram_helper.h"
#include "components/enterprise/data_controls/core/browser/dlp_policy_event.pb.h"
#include "components/file_access/scoped_file_access.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "url/gurl.h"

using ::base::test::EqualsProto;
using blink::mojom::FileSystemFileInfo;
using blink::mojom::NativeFileInfo;
using storage::FileSystemURL;
using testing::_;
using testing::Mock;

namespace policy {

namespace {

// System application URLs.
// Please keep them updated with dlp_files_controller_ash.cc.
constexpr char kFileManagerUrl[] = "chrome://file-manager/";
constexpr char kImageLoaderUrl[] =
    "chrome-extension://pmfjbimdmchhbnneeidfognadeopoehp/";

constexpr char kExampleUrl1[] = "https://1.example.com/";
constexpr char kExampleUrl2[] = "https://2.example.com/";
constexpr char kExampleUrl3[] = "https://3.example.com/";
constexpr char kExampleUrl4[] = "https://4.example.com/";
constexpr char kExampleUrl5[] = "https://5.example.com/";
constexpr char kExampleUrl6[] = "https://6.example.com/";
constexpr char kExampleUrl7[] = "https://7.example.com/";

constexpr char kExampleSourceUrl1[] = "1.example.com";
constexpr char kExampleSourceUrl2[] = "2.example.com";
constexpr char kExampleSourceUrl3[] = "3.example.com";
constexpr char kExampleSourceUrl4[] = "4.example.com";

constexpr char kReferrerUrl1[] = "https://referrer1.com/";
constexpr char kReferrerUrl2[] = "https://referrer2.com/";
constexpr char kReferrerUrl3[] = "https://referrer3.com/";
constexpr char kReferrerUrl4[] = "https://referrer4.com/";
constexpr char kReferrerUrl5[] = "https://referrer5.com/";

constexpr ino64_t kInode1 = 1;
constexpr ino64_t kInode2 = 2;
constexpr ino64_t kInode3 = 3;
constexpr ino64_t kInode4 = 4;
constexpr ino64_t kInode5 = 5;
constexpr time_t kCrtime1 = 1;
constexpr time_t kCrtime2 = 2;
constexpr time_t kCrtime3 = 3;
constexpr time_t kCrtime4 = 4;
constexpr time_t kCrtime5 = 5;

constexpr char kFilePath1[] = "test1.txt";
constexpr char kFilePath2[] = "test2.txt";
constexpr char kFilePath3[] = "test3.txt";
constexpr char kFilePath4[] = "test4.txt";
constexpr char kFilePath5[] = "test5.txt";

constexpr char kStandaloneBrowserChromeAppId[] = "standaloneChromeApp";
constexpr char kExtensionAppId[] = "extensionApp";
constexpr char kStandaloneBrowserExtensionAppId[] =
    "standaloneBrowserExtensionApp";
constexpr char kChromeAppId[] = "chromeApp";
constexpr char kArcAppId[] = "arcApp";
constexpr char kCrostiniAppId[] = "crostiniApp";
constexpr char kPluginVmAppId[] = "pluginVmApp";
constexpr char kWebAppId[] = "webApp";
constexpr char kSystemWebAppId[] = "systemWebApp";
constexpr char kUnknownAppId[] = "unknownApp";
constexpr char kBuiltInAppId[] = "builtInApp";
constexpr char kStandaloneBrowserAppId[] = "standaloneBrowserApp";
constexpr char kRemoteAppId[] = "remoteApp";
constexpr char kBorealisAppId[] = "borealisApp";
constexpr char kBruschettaAppId[] = "bruschettaApp";

constexpr char kRuleName1[] = "rule #1";
constexpr char kRuleName2[] = "rule #2";
constexpr char kRuleName3[] = "rule #3";
constexpr char kRuleName4[] = "rule #4";
constexpr char kRuleId1[] = "testid1";
constexpr char kRuleId2[] = "testid2";
constexpr char kRuleId3[] = "testid3";
constexpr char kRuleId4[] = "testid4";
const DlpRulesManager::RuleMetadata kRuleMetadata1(kRuleName1, kRuleId1);
const DlpRulesManager::RuleMetadata kRuleMetadata2(kRuleName2, kRuleId2);
const DlpRulesManager::RuleMetadata kRuleMetadata3(kRuleName3, kRuleId3);
const DlpRulesManager::RuleMetadata kRuleMetadata4(kRuleName4, kRuleId4);


// For a given |root| converts the given virtual |path| to a GURL.
GURL ToGURL(const base::FilePath& root, const std::string& path) {
  const std::string abs_path = root.Append(path).value();
  return GURL(base::StrCat({url::kFileSystemScheme, ":",
                            file_manager::util::GetFilesAppOrigin().Serialize(),
                            abs_path}));
}

struct FilesTransferInfo {
  FilesTransferInfo(policy::dlp::FileAction files_action,
                    std::vector<ino_t> file_inodes,
                    std::vector<time_t> file_crtimes,
                    std::vector<std::string> file_sources,
                    std::vector<std::string> file_paths)
      : files_action(files_action),
        file_inodes(file_inodes),
        file_crtimes(file_crtimes),
        file_sources(file_sources),
        file_paths(file_paths) {}

  policy::dlp::FileAction files_action;
  std::vector<ino_t> file_inodes;
  std::vector<time_t> file_crtimes;
  std::vector<std::string> file_sources;
  std::vector<std::string> file_paths;
};

using MockIsFilesTransferRestrictedCallback =
    testing::StrictMock<base::MockCallback<
        DlpFilesControllerAsh::IsFilesTransferRestrictedCallback>>;
using MockCheckIfDlpAllowedCallback = testing::StrictMock<
    base::MockCallback<DlpFilesControllerAsh::CheckIfDlpAllowedCallback>>;
using MockGetFilesSources =
    testing::StrictMock<base::MockCallback<base::RepeatingCallback<void(
        ::dlp::GetFilesSourcesRequest,
        ::chromeos::DlpClient::GetFilesSourcesCallback)>>>;
using MockAddFiles = testing::StrictMock<base::MockCallback<
    base::RepeatingCallback<void(::dlp::AddFilesRequest,
                                 ::chromeos::DlpClient::AddFilesCallback)>>>;
using FileDaemonInfo = policy::DlpFilesControllerAsh::FileDaemonInfo;

}  // namespace

class DlpFilesControllerAshTest : public DlpFilesTestWithMounts {
 public:
  DlpFilesControllerAshTest(const DlpFilesControllerAshTest&) = delete;
  DlpFilesControllerAshTest& operator=(const DlpFilesControllerAshTest&) =
      delete;

 protected:
  DlpFilesControllerAshTest() = default;
  ~DlpFilesControllerAshTest() override = default;

  void AddFilesToDlpClient(std::vector<FileDaemonInfo> files,
                           std::vector<FileSystemURL>& out_files_urls) {
    ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());

    base::MockCallback<chromeos::DlpClient::AddFilesCallback> add_files_cb;
    EXPECT_CALL(add_files_cb, Run(testing::_)).Times(1);

    ::dlp::AddFilesRequest request;
    for (const auto& file : files) {
      ASSERT_TRUE(CreateDummyFile(file.path));
      ::dlp::AddFileRequest* add_file_req = request.add_add_file_requests();
      add_file_req->set_file_path(file.path.value());
      add_file_req->set_source_url(file.source_url.spec());
      add_file_req->set_referrer_url(file.referrer_url.spec());

      auto file_url = CreateFileSystemURL(kTestStorageKey, file.path.value());
      ASSERT_TRUE(file_url.is_valid());
      out_files_urls.push_back(std::move(file_url));
    }
    chromeos::DlpClient::Get()->AddFiles(request, add_files_cb.Get());
    testing::Mock::VerifyAndClearExpectations(&add_files_cb);
  }
};

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_DiffFileSystem) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewFilesPolicyUX);

  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<FileSystemURL> transferred_files(
      {files_urls[0], files_urls[1], files_urls[2]});
  std::vector<FileSystemURL> disallowed_files({files_urls[0], files_urls[2]});

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  for (const auto& file : disallowed_files) {
    check_files_transfer_response.add_files_paths(file.path().value());
  }
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  mount_points_->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kArchiveMountPath));
  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), "archive",
      base::FilePath("file.rar/path/in/archive"));

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{1},
                          std::vector<base::FilePath>{files_urls[0].path(),
                                                      files_urls[2].path()},
                          dlp::FileAction::kMove));

  base::test::TestFuture<std::vector<FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfTransferAllowed(/*task_id=*/1, transferred_files,
                                            dst_url, /*is_move=*/true,
                                            future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(disallowed_files, future.Take());

  // Validate the request sent to the daemon.
  std::vector<std::string> expected_requested_files;
  for (const auto& file_url : files_urls) {
    expected_requested_files.push_back(file_url.path().value());
  }

  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  std::vector<std::string> requested_files(request.files_paths().begin(),
                                           request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
  EXPECT_EQ(::dlp::DlpComponent::SYSTEM, request.destination_component());
  EXPECT_EQ(::dlp::FileAction::MOVE, request.file_action());
  EXPECT_EQ(1u, request.io_task_id());
}

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_SameFileSystem) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> transferred_files(
      {files_urls[0], files_urls[1], files_urls[2]});

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfTransferAllowed(
      /*task_id=*/std::nullopt, transferred_files,
      CreateFileSystemURL(kTestStorageKey, "Downloads"),
      /*is_move=*/false, future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_ClientNotRunning) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> transferred_files(
      {files_urls[0], files_urls[1], files_urls[2]});

  mount_points_->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kArchiveMountPath));
  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), "archive",
      base::FilePath("file.rar/path/in/archive"));

  chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(false);
  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfTransferAllowed(
      /*task_id=*/std::nullopt, transferred_files, dst_url, /*is_move=*/true,
      future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_ErrorResponse) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> transferred_files(
      {files_urls[0], files_urls[1], files_urls[2]});

  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  mount_points->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kArchiveMountPath));
  absl::Cleanup external_mount_points_revoker = [mount_points] {
    mount_points->RevokeAllFileSystems();
  };

  auto dst_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey(), "archive",
      base::FilePath("file.rar/path/in/archive"));

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(files_urls[0].path().value());
  check_files_transfer_response.add_files_paths(files_urls[2].path().value());
  check_files_transfer_response.set_error_message("Did not receive a reply.");
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfTransferAllowed(
      /*task_id=*/std::nullopt, transferred_files, dst_url,
      /*is_move=*/false, future.GetCallback());

  ASSERT_EQ(3u, future.Get().size());
  EXPECT_EQ(transferred_files, future.Take());

  // Validate the request sent to the daemon.
  std::vector<std::string> expected_requested_files;
  for (const auto& file_url : files_urls) {
    expected_requested_files.push_back(file_url.path().value());
  }

  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  std::vector<std::string> requested_files(request.files_paths().begin(),
                                           request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
  EXPECT_EQ(::dlp::DlpComponent::SYSTEM, request.destination_component());
  EXPECT_EQ(::dlp::FileAction::COPY, request.file_action());
  EXPECT_FALSE(request.has_io_task_id());
}

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_MultiFolder) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kNewFilesPolicyUX);

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(my_files_dir_));
  base::ScopedTempDir sub_dir2;
  ASSERT_TRUE(sub_dir2.CreateUniqueTempDirUnderPath(sub_dir1.GetPath()));
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1,
                     sub_dir1.GetPath().AppendASCII(kFilePath1), kExampleUrl1,
                     kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2,
                     sub_dir1.GetPath().AppendASCII(kFilePath2), kExampleUrl2,
                     kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3,
                     sub_dir1.GetPath().AppendASCII(kFilePath3), kExampleUrl3,
                     kReferrerUrl3),
      FileDaemonInfo(kInode4, kCrtime4,
                     sub_dir2.GetPath().AppendASCII(kFilePath4), kExampleUrl4,
                     kReferrerUrl4),
      FileDaemonInfo(kInode5, kCrtime5,
                     sub_dir2.GetPath().AppendASCII(kFilePath5), kExampleUrl5,
                     kReferrerUrl5)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> transferred_files(
      {CreateFileSystemURL(kTestStorageKey, sub_dir1.GetPath().value())});

  mount_points_->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kArchiveMountPath));
  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), "archive",
      base::FilePath("file.rar/path/in/archive"));

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(files_urls[1].path().value());
  check_files_transfer_response.add_files_paths(files_urls[2].path().value());
  check_files_transfer_response.add_files_paths(files_urls[4].path().value());
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{1},
                          std::vector<base::FilePath>{files_urls[1].path(),
                                                      files_urls[2].path(),
                                                      files_urls[4].path()},
                          dlp::FileAction::kCopy));

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfTransferAllowed(/*task_id=*/1, transferred_files,
                                            dst_url, /*is_move=*/false,
                                            future.GetCallback());

  std::vector<storage::FileSystemURL> expected_restricted_files(
      {files_urls[1], files_urls[2], files_urls[4]});
  ASSERT_EQ(3u, future.Get().size());
  EXPECT_EQ(expected_restricted_files, future.Take());

  // Validate the request sent to the daemon.
  std::vector<std::string> expected_requested_files;
  for (const auto& file_url : files_urls) {
    expected_requested_files.push_back(file_url.path().value());
  }

  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  std::vector<std::string> requested_files(request.files_paths().begin(),
                                           request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
  EXPECT_EQ(::dlp::DlpComponent::SYSTEM, request.destination_component());
  EXPECT_EQ(::dlp::FileAction::COPY, request.file_action());
  EXPECT_EQ(1u, request.io_task_id());
}

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_ExternalFiles) {
  base::ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());
  base::FilePath file_path1 = external_dir.GetPath().AppendASCII(kFilePath1);
  ASSERT_TRUE(CreateDummyFile(file_path1));
  auto file_url1 = CreateFileSystemURL(kTestStorageKey, file_path1.value());
  base::FilePath file_path2 = external_dir.GetPath().AppendASCII(kFilePath2);
  ASSERT_TRUE(CreateDummyFile(file_path2));
  auto file_url2 = CreateFileSystemURL(kTestStorageKey, file_path2.value());

  std::vector<FileSystemURL> transferred_files({file_url1, file_url2});
  base::test::TestFuture<std::vector<FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfTransferAllowed(
      /*task_id=*/std::nullopt, transferred_files, my_files_dir_url_,
      /*is_move=*/true, future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::vector<FileSystemURL>(), future.Take());

  // Validate that no files were requested from the daemon.
  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  ASSERT_EQ(request.files_paths().size(), 0);
}

TEST_F(DlpFilesControllerAshTest, FilterDisallowedUploads_EmptyList) {
  base::test::TestFuture<std::vector<ui::SelectedFileInfo>> future;

  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(
      {}, DlpFileDestination(GURL(kExampleUrl1)), future.GetCallback());

  std::vector<ui::SelectedFileInfo> filtered_uploads;

  ASSERT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerAshTest, FilterDisallowedUploads_MixedFiles) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<ui::SelectedFileInfo> uploaded_files;
  uploaded_files.emplace_back(files_urls[0].path(), files_urls[0].path());
  uploaded_files.emplace_back(files_urls[1].path(), files_urls[1].path());
  uploaded_files.emplace_back(files_urls[2].path(), files_urls[2].path());

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(files_urls[0].path().value());
  check_files_transfer_response.add_files_paths(files_urls[2].path().value());
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{files_urls[0].path(),
                                                      files_urls[2].path()},
                          dlp::FileAction::kUpload));

  base::test::TestFuture<std::vector<ui::SelectedFileInfo>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(
      std::move(uploaded_files), DlpFileDestination(GURL(kExampleUrl1)),
      future.GetCallback());

  std::vector<ui::SelectedFileInfo> filtered_uploads;
  filtered_uploads.emplace_back(files_urls[1].path(), files_urls[1].path());

  ASSERT_EQ(1u, future.Get().size());
  EXPECT_EQ(filtered_uploads, future.Take());

  // Validate the request sent to the daemon.
  std::vector<std::string> expected_requested_files;
  for (const auto& file_url : files_urls) {
    expected_requested_files.push_back(file_url.path().value());
  }

  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  std::vector<std::string> requested_files(request.files_paths().begin(),
                                           request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
  EXPECT_EQ(kExampleUrl1, request.destination_url());
  EXPECT_EQ(::dlp::FileAction::UPLOAD, request.file_action());
  EXPECT_FALSE(request.has_io_task_id());
}

TEST_F(DlpFilesControllerAshTest, CheckIfTransferAllowed_NoFileSystemContext) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> transferred_files(
      {files_urls[0], files_urls[1], files_urls[2]});

  mount_points_->RegisterFileSystem(
      ash::kSystemMountNameArchive, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kArchiveMountPath));
  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), "archive",
      base::FilePath("file.rar/path/in/archive"));

  base::test::TestFuture<std::vector<storage::FileSystemURL>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->SetFileSystemContextForTesting(nullptr);
  files_controller_->CheckIfTransferAllowed(
      /*task_id=*/std::nullopt, transferred_files, dst_url, /*is_move=*/true,
      future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(DlpFilesControllerAshTest, FilterDisallowedUploads_ErrorResponse) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<ui::SelectedFileInfo> selected_files;
  selected_files.emplace_back(files_urls[0].path(), files_urls[0].path());
  selected_files.emplace_back(files_urls[1].path(), files_urls[1].path());
  selected_files.emplace_back(files_urls[2].path(), files_urls[2].path());

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(files_urls[0].path().value());
  check_files_transfer_response.add_files_paths(files_urls[2].path().value());
  check_files_transfer_response.set_error_message("Did not receive a reply.");
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  base::test::TestFuture<std::vector<ui::SelectedFileInfo>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(
      selected_files, DlpFileDestination(data_controls::Component::kArc),
      future.GetCallback());

  ASSERT_EQ(3u, future.Get().size());
  EXPECT_EQ(selected_files, future.Take());

  // Validate the request sent to the daemon.
  std::vector<std::string> expected_requested_files;
  for (const auto& file_url : files_urls) {
    expected_requested_files.push_back(file_url.path().value());
  }

  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  std::vector<std::string> requested_files(request.files_paths().begin(),
                                           request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
  EXPECT_EQ(::dlp::ARC, request.destination_component());
  EXPECT_EQ(::dlp::FileAction::UPLOAD, request.file_action());
  EXPECT_FALSE(request.has_io_task_id());
}

TEST_F(DlpFilesControllerAshTest, FilterDisallowedUploads_MultiFolder) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(my_files_dir_));
  base::ScopedTempDir sub_dir2;
  ASSERT_TRUE(sub_dir2.CreateUniqueTempDirUnderPath(my_files_dir_));
  base::ScopedTempDir sub_dir2_1;
  ASSERT_TRUE(sub_dir2_1.CreateUniqueTempDirUnderPath(sub_dir2.GetPath()));
  base::ScopedTempDir sub_dir3;
  ASSERT_TRUE(sub_dir3.CreateUniqueTempDirUnderPath(my_files_dir_));
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1,
                     sub_dir1.GetPath().AppendASCII(kFilePath1), kExampleUrl1,
                     kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2,
                     sub_dir1.GetPath().AppendASCII(kFilePath2), kExampleUrl2,
                     kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3,
                     sub_dir2_1.GetPath().AppendASCII(kFilePath3), kExampleUrl3,
                     kReferrerUrl3),
      FileDaemonInfo(kInode4, kCrtime4,
                     sub_dir2_1.GetPath().AppendASCII(kFilePath4), kExampleUrl4,
                     kReferrerUrl4),
      FileDaemonInfo(kInode5, kCrtime5,
                     sub_dir3.GetPath().AppendASCII(kFilePath5), kExampleUrl5,
                     kReferrerUrl5)};
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<ui::SelectedFileInfo> selected_files(
      {{sub_dir1.GetPath(), sub_dir1.GetPath()},
       {sub_dir2_1.GetPath(), sub_dir2_1.GetPath()},
       {sub_dir3.GetPath(), sub_dir3.GetPath()}});

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(files_urls[0].path().value());
  check_files_transfer_response.add_files_paths(files_urls[1].path().value());
  check_files_transfer_response.add_files_paths(files_urls[3].path().value());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{files_urls[0].path(),
                                                      files_urls[1].path(),
                                                      files_urls[3].path()},
                          dlp::FileAction::kUpload));

  base::test::TestFuture<std::vector<ui::SelectedFileInfo>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->FilterDisallowedUploads(
      selected_files, DlpFileDestination(GURL(kExampleUrl1)),
      future.GetCallback());

  std::vector<ui::SelectedFileInfo> expected_filtered_uploads(
      {{sub_dir3.GetPath(), sub_dir3.GetPath()}});
  ASSERT_EQ(1u, future.Get().size());
  EXPECT_EQ(expected_filtered_uploads, future.Take());

  // Validate the request sent to the daemon.
  std::vector<std::string> expected_requested_files;
  for (const auto& file_url : files_urls) {
    expected_requested_files.push_back(file_url.path().value());
  }

  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  std::vector<std::string> requested_files(request.files_paths().begin(),
                                           request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
  EXPECT_EQ(kExampleUrl1, request.destination_url());
  EXPECT_EQ(::dlp::FileAction::UPLOAD, request.file_action());
  EXPECT_FALSE(request.has_io_task_id());
}

TEST_F(DlpFilesControllerAshTest, FilterDisallowedUploads_NoFileSystemContext) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<ui::SelectedFileInfo> selected_files;
  selected_files.emplace_back(files_urls[0].path(), files_urls[0].path());
  selected_files.emplace_back(files_urls[1].path(), files_urls[1].path());
  selected_files.emplace_back(files_urls[2].path(), files_urls[2].path());

  base::test::TestFuture<std::vector<ui::SelectedFileInfo>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->SetFileSystemContextForTesting(nullptr);
  files_controller_->FilterDisallowedUploads(
      selected_files, DlpFileDestination(GURL(kExampleUrl1)),
      future.GetCallback());

  ASSERT_EQ(3u, future.Get().size());
  EXPECT_EQ(selected_files, future.Take());
}

TEST_F(DlpFilesControllerAshTest, GetDlpMetadata) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> files_to_check(
      {files_urls[0], files_urls[1], files_urls[2]});
  std::vector<DlpFilesControllerAsh::DlpFileMetadata> dlp_metadata(
      {DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl1, kReferrerUrl1, /*is_dlp_restricted=*/true,
           /*is_restricted_for_destination=*/false),
       DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl2, kReferrerUrl2, /*is_dlp_restricted=*/false,
           /*is_restricted_for_destination=*/false),
       DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl3, kReferrerUrl3, /*is_dlp_restricted=*/true,
           /*is_restricted_for_destination=*/false)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn));
  // If destination is not passed, neither of these should be called.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination).Times(0);
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent).Times(0);

  base::test::TestFuture<std::vector<DlpFilesControllerAsh::DlpFileMetadata>>
      future;
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(files_to_check, std::nullopt,
                                    future.GetCallback());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerAshTest, GetDlpMetadata_WithComponent) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> files_to_check(
      {files_urls[0], files_urls[1], files_urls[2]});
  std::vector<DlpFilesControllerAsh::DlpFileMetadata> dlp_metadata(
      {DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl1, kReferrerUrl1, /*is_dlp_restricted=*/true,
           /*is_restricted_for_destination=*/true),
       DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl2, kReferrerUrl2, /*is_dlp_restricted=*/false,
           /*is_restricted_for_destination=*/false),
       DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl3, kReferrerUrl3, /*is_dlp_restricted=*/true,
           /*is_restricted_for_destination=*/false)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  // If destination is passed as component, the restriction should be checked if
  // there are files with any "block" restriction.
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn))
      .RetiresOnSaturation();
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination).Times(0);

  base::test::TestFuture<std::vector<DlpFilesControllerAsh::DlpFileMetadata>>
      future;
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(
      files_to_check, DlpFileDestination(data_controls::Component::kUsb),
      future.GetCallback());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerAshTest, GetDlpMetadata_WithDestination) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> files_to_check(
      {files_urls[0], files_urls[1], files_urls[2]});
  std::vector<DlpFilesControllerAsh::DlpFileMetadata> dlp_metadata(
      {DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl1, kReferrerUrl1, /*is_dlp_restricted=*/true,
           /*is_restricted_for_destination=*/true),
       DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl2, kReferrerUrl2, /*is_dlp_restricted=*/false,
           /*is_restricted_for_destination=*/false),
       DlpFilesControllerAsh::DlpFileMetadata(
           kExampleUrl3, kReferrerUrl3, /*is_dlp_restricted=*/true,
           /*is_restricted_for_destination=*/false)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kAllow))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));
  // If destination is passed as url, the restriction should be checked if there
  // are files with any "block" restriction.
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn))
      .RetiresOnSaturation();
  EXPECT_CALL(*rules_manager_, IsRestrictedComponent).Times(0);

  base::test::TestFuture<std::vector<DlpFilesControllerAsh::DlpFileMetadata>>
      future;
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(files_to_check,
                                    DlpFileDestination(GURL(kExampleUrl1)),
                                    future.GetCallback());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerAshTest, GetDlpMetadata_FileNotAvailable) {
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());

  auto file_path = my_files_dir_.AppendASCII(kFilePath1);
  ASSERT_TRUE(CreateDummyFile(file_path));
  auto file_url = CreateFileSystemURL(kTestStorageKey, file_path.value());
  ASSERT_TRUE(file_url.is_valid());

  std::vector<storage::FileSystemURL> files_to_check({file_url});
  std::vector<DlpFilesControllerAsh::DlpFileMetadata> dlp_metadata(
      {DlpFilesControllerAsh::DlpFileMetadata(
          /*source_url=*/"", /*referrer_url=*/"", /*is_dlp_restricted=*/false,
          /*is_restricted_for_destination=*/false)});

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule).Times(0);

  base::test::TestFuture<std::vector<DlpFilesControllerAsh::DlpFileMetadata>>
      future;
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(files_to_check, std::nullopt,
                                    future.GetCallback());
  EXPECT_EQ(dlp_metadata, future.Take());
}

TEST_F(DlpFilesControllerAshTest, GetDlpMetadata_ClientNotRunning) {
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<storage::FileSystemURL> files_to_check(
      {files_urls[0], files_urls[1], files_urls[2]});

  base::test::TestFuture<std::vector<DlpFilesControllerAsh::DlpFileMetadata>>
      future;
  chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(false);
  ASSERT_TRUE(files_controller_);
  files_controller_->GetDlpMetadata(files_to_check, std::nullopt,
                                    future.GetCallback());
  EXPECT_EQ(std::vector<DlpFilesControllerAsh::DlpFileMetadata>(),
            future.Take());
}

TEST_F(DlpFilesControllerAshTest, GetDlpRestrictionDetails_Mixed) {
  DlpRulesManager::AggregatedDestinations destinations;
  destinations[DlpRulesManager::Level::kBlock].insert(kExampleUrl2);
  destinations[DlpRulesManager::Level::kAllow].insert(kExampleUrl3);

  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kUsb);
  components[DlpRulesManager::Level::kWarn].insert(
      data_controls::Component::kDrive);
  components[DlpRulesManager::Level::kReport].insert(
      data_controls::Component::kOneDrive);

  EXPECT_CALL(*rules_manager_, GetAggregatedDestinations)
      .WillOnce(testing::Return(destinations));
  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  ASSERT_TRUE(files_controller_);
  auto result = files_controller_->GetDlpRestrictionDetails(kExampleUrl1);

  ASSERT_EQ(result.size(), 4u);
  std::vector<std::string> expected_urls;
  std::vector<data_controls::Component> expected_components;
  // Block:
  expected_urls.push_back(kExampleUrl2);
  expected_components.push_back(data_controls::Component::kUsb);
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
  // Report:
  expected_urls.clear();
  expected_components.clear();
  expected_components.push_back(data_controls::Component::kOneDrive);
  EXPECT_EQ(result[2].level, DlpRulesManager::Level::kReport);
  EXPECT_EQ(result[2].urls, expected_urls);
  EXPECT_EQ(result[2].components, expected_components);
  // Warn:
  expected_urls.clear();
  expected_components.clear();
  expected_components.push_back(data_controls::Component::kDrive);
  EXPECT_EQ(result[3].level, DlpRulesManager::Level::kWarn);
  EXPECT_EQ(result[3].urls, expected_urls);
  EXPECT_EQ(result[3].components, expected_components);
}

TEST_F(DlpFilesControllerAshTest, GetDlpRestrictionDetails_Components) {
  DlpRulesManager::AggregatedDestinations destinations;
  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kUsb);

  EXPECT_CALL(*rules_manager_, GetAggregatedDestinations)
      .WillOnce(testing::Return(destinations));
  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  ASSERT_TRUE(files_controller_);
  auto result = files_controller_->GetDlpRestrictionDetails(kExampleUrl1);
  ASSERT_EQ(result.size(), 1u);
  std::vector<std::string> expected_urls;
  std::vector<data_controls::Component> expected_components;
  expected_components.push_back(data_controls::Component::kUsb);
  EXPECT_EQ(result[0].level, DlpRulesManager::Level::kBlock);
  EXPECT_EQ(result[0].urls, expected_urls);
  EXPECT_EQ(result[0].components, expected_components);
}

TEST_F(DlpFilesControllerAshTest, GetBlockedComponents) {
  DlpRulesManager::AggregatedComponents components;
  components[DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kArc);
  components[DlpRulesManager::Level::kBlock].insert(
      data_controls::Component::kCrostini);
  components[DlpRulesManager::Level::kWarn].insert(
      data_controls::Component::kUsb);
  components[DlpRulesManager::Level::kReport].insert(
      data_controls::Component::kDrive);

  EXPECT_CALL(*rules_manager_, GetAggregatedComponents)
      .WillOnce(testing::Return(components));

  ASSERT_TRUE(files_controller_);
  auto result = files_controller_->GetBlockedComponents(kExampleUrl1);
  ASSERT_EQ(result.size(), 2u);
  std::vector<data_controls::Component> expected_components;
  expected_components.push_back(data_controls::Component::kArc);
  expected_components.push_back(data_controls::Component::kCrostini);
  EXPECT_EQ(result, expected_components);
}

TEST_F(DlpFilesControllerAshTest, DownloadToLocalAllowed) {
  base::test::TestFuture<bool> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfDownloadAllowed(
      DlpFileDestination(GURL(kExampleUrl1)),
      base::FilePath(
          "/home/chronos/u-0123456789abcdef/MyFiles/Downloads/img.jpg"),
      future.GetCallback());
  EXPECT_TRUE(future.Take());
}

TEST_F(DlpFilesControllerAshTest, DownloadFromComponentAllowed) {
  base::test::TestFuture<bool> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfDownloadAllowed(
      DlpFileDestination(data_controls::Component::kArc),
      base::FilePath("path/in/android/filename"), future.GetCallback());
  EXPECT_TRUE(future.Take());
}

TEST_F(DlpFilesControllerAshTest, FilePromptForDownloadLocal) {
  ASSERT_TRUE(files_controller_);
  EXPECT_FALSE(files_controller_->ShouldPromptBeforeDownload(
      DlpFileDestination(GURL(kExampleUrl1)),
      base::FilePath(
          "/home/chronos/u-0123456789abcdef/MyFiles/Downloads/img.jpg")));
}

TEST_F(DlpFilesControllerAshTest, FilePromptForDownloadNoSource) {
  ASSERT_TRUE(files_controller_);
  EXPECT_FALSE(files_controller_->ShouldPromptBeforeDownload(
      DlpFileDestination(), base::FilePath("path/in/android/filename")));
}

TEST_F(DlpFilesControllerAshTest, CheckReportingOnIsDlpPolicyMatched) {
  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(kExampleSourceUrl1),
                               testing::SetArgPointee<3>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<2>(kExampleSourceUrl2),
                         testing::SetArgPointee<3>(kRuleMetadata2),
                         testing::Return(DlpRulesManager::Level::kReport)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<2>(kExampleSourceUrl3),
                               testing::SetArgPointee<3>(kRuleMetadata3),
                               testing::Return(DlpRulesManager::Level::kWarn)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<2>(kExampleSourceUrl4),
                         testing::SetArgPointee<3>(kRuleMetadata4),
                         testing::Return(DlpRulesManager::Level::kAllow)));

  EXPECT_CALL(*rules_manager_, GetReportingManager).Times(testing::AnyNumber());

  const auto histogram_tester = base::HistogramTester();

  const auto file1 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(kFilePath1), kExampleUrl1,
      kReferrerUrl1);
  const auto file2 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode2, kCrtime2, base::FilePath(kFilePath2), kExampleUrl2,
      kReferrerUrl2);
  const auto file3 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode3, kCrtime3, base::FilePath(kFilePath3), kExampleUrl3,
      kReferrerUrl3);
  const auto file4 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode4, kCrtime4, base::FilePath(kFilePath4), kExampleUrl4,
      kReferrerUrl4);

  ASSERT_TRUE(files_controller_->IsDlpPolicyMatched(file1));
  ASSERT_FALSE(files_controller_->IsDlpPolicyMatched(file2));
  ASSERT_TRUE(files_controller_->IsDlpPolicyMatched(file3));
  ASSERT_FALSE(files_controller_->IsDlpPolicyMatched(file4));

  ASSERT_EQ(events_.size(), 0u);
}

TEST_F(DlpFilesControllerAshTest, CheckReportingOnIsFilesTransferRestricted) {
  const auto histogram_tester = base::HistogramTester();

  const auto file1 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(kFilePath1), kExampleUrl1,
      kReferrerUrl1);
  const auto file2 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode2, kCrtime2, base::FilePath(kFilePath2), kExampleUrl2,
      kReferrerUrl2);

  const GURL dst_url("https://wetransfer.com/");

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination(_, _, _, _, _, _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<5>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                               testing::SetArgPointee<5>(kRuleMetadata2),
                               testing::Return(DlpRulesManager::Level::kAllow)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<5>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                               testing::SetArgPointee<5>(kRuleMetadata2),
                               testing::Return(DlpRulesManager::Level::kAllow)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<5>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                         testing::SetArgPointee<5>(kRuleMetadata2),
                         testing::Return(DlpRulesManager::Level::kAllow)));

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, data_controls::Component::kUsb, _, _, _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<4>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                               testing::SetArgPointee<4>(kRuleMetadata2),
                               testing::Return(DlpRulesManager::Level::kAllow)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<4>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                               testing::SetArgPointee<4>(kRuleMetadata2),
                               testing::Return(DlpRulesManager::Level::kAllow)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<4>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                         testing::SetArgPointee<4>(kRuleMetadata2),
                         testing::Return(DlpRulesManager::Level::kAllow)));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  std::vector<DlpFilesControllerAsh::FileDaemonInfo> transferred_files = {
      file1, file2};
  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels =
      {{file1, ::dlp::RestrictionLevel::LEVEL_BLOCK},
       {file2, ::dlp::RestrictionLevel::LEVEL_ALLOW}};
  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(files_levels)).Times(::testing::AnyNumber());

  auto event_builder = data_controls::DlpPolicyEventBuilder::Event(
      kExampleUrl1, kRuleName1, kRuleId1, DlpRulesManager::Restriction::kFiles,
      DlpRulesManager::Level::kBlock);
  event_builder->SetContentName(kFilePath1);

  event_builder->SetDestinationUrl(dst_url.spec());
  const auto event1 = event_builder->Create();

  event_builder->SetDestinationComponent(data_controls::Component::kUsb);
  const auto event2 = event_builder->Create();

  base::TimeDelta cooldown_time =
      event_storage_->GetDeduplicationCooldownForTesting();

  std::vector<base::TimeDelta> delays = {cooldown_time / 2, cooldown_time,
                                         base::Seconds(0)};

  for (base::TimeDelta delay : delays) {
    // Report `event1` after this call if `delay` is at least `cooldown_time`.
    files_controller_->IsFilesTransferRestricted(
        /*task_id=*/1234, transferred_files, DlpFileDestination(dst_url),
        dlp::FileAction::kTransfer, cb.Get());

    // Report `event2` after this call if `delay` is at least `cooldown_time`.
    files_controller_->IsFilesTransferRestricted(
        /*task_id=*/1234, transferred_files,
        DlpFileDestination(data_controls::Component::kUsb),
        dlp::FileAction::kTransfer, cb.Get());

    task_runner_->FastForwardBy(delay);
  }

  const auto expected_events =
      std::vector<const DlpPolicyEvent*>({&event1, &event2, &event1, &event2});

  ASSERT_EQ(events_.size(), 4u);
  for (size_t i = 0; i < events_.size(); ++i) {
    EXPECT_THAT(events_[i],
                data_controls::IsDlpPolicyEvent(*expected_events[i]));
  }
}

TEST_F(DlpFilesControllerAshTest, CheckReportingOnMixedCalls) {
  const auto file1 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(kFilePath1), kExampleUrl1,
      kReferrerUrl1);
  const auto file2 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode2, kCrtime2, base::FilePath(kFilePath2), kExampleUrl2,
      kReferrerUrl2);

  const GURL dst_url("https://wetransfer.com/");

  EXPECT_CALL(*rules_manager_, IsRestrictedByAnyRule)
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<2>(kExampleUrl1),
                         testing::Return(DlpRulesManager::Level::kBlock)));

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination(_, _, _, _, _, _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<5>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                         testing::SetArgPointee<5>(kRuleMetadata2),
                         testing::Return(DlpRulesManager::Level::kAllow)));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  std::vector<DlpFilesControllerAsh::FileDaemonInfo> transferred_files = {
      file1, file2};
  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels =
      {{file1, ::dlp::RestrictionLevel::LEVEL_BLOCK},
       {file2, ::dlp::RestrictionLevel::LEVEL_ALLOW}};

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(files_levels)).Times(1);

  auto event_builder = data_controls::DlpPolicyEventBuilder::Event(
      GURL(kExampleUrl1).spec(), kRuleName1, kRuleId1,
      DlpRulesManager::Restriction::kFiles, DlpRulesManager::Level::kBlock);
  event_builder->SetContentName(kFilePath1);
  event_builder->SetDestinationUrl(dst_url.spec());
  const auto event = event_builder->Create();

  // Report a single `event` after this call
  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/2345, transferred_files, DlpFileDestination(dst_url),
      dlp::FileAction::kTransfer, cb.Get());

  // Do not report after these calls
  ASSERT_TRUE(files_controller_->IsDlpPolicyMatched(file1));

  ASSERT_EQ(events_.size(), 1u);
  EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(event));
}

// Test that no file event is generated for a selected list of system apps.
// These apps may access files without a user-initiated action (e.g.,
// thumbnails loaded when Files app is opened). We should probably not
// report in these scenarios.
TEST_F(DlpFilesControllerAshTest, DoNotReportOnSystemApps) {
  const auto histogram_tester = base::HistogramTester();

  const auto file = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(kFilePath1), kExampleUrl1,
      kReferrerUrl1);

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination(_, _, _, _, _, _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleSourceUrl1),
                               testing::SetArgPointee<5>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleSourceUrl1),
                         testing::SetArgPointee<5>(kRuleMetadata1),
                         testing::Return(DlpRulesManager::Level::kBlock)));

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/1234, {file}, DlpFileDestination(GURL(kFileManagerUrl)),
      dlp::FileAction::kTransfer, base::DoNothing());

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/1234, {file}, DlpFileDestination(GURL(kImageLoaderUrl)),
      dlp::FileAction::kTransfer, base::DoNothing());

  EXPECT_THAT(histogram_tester.GetAllSamples(
                  data_controls::GetDlpHistogramPrefix() +
                  std::string(data_controls::dlp::kFileActionBlockedUMA)),
              base::BucketsAre(base::Bucket(dlp::FileAction::kTransfer, 0)));
}

// Warnings to Files app and image loader should be converted to blocks to avoid
// mass warnings when browsing a folder with warned images.
TEST_F(DlpFilesControllerAshTest, BlockWarningFilesOnSystemApps) {
  const auto file = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(kFilePath1), kExampleUrl1,
      kReferrerUrl1);

  base::MockOnceCallback<void(
      const std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>>&)>
      result_callback;

  EXPECT_CALL(
      result_callback,
      Run(testing::ElementsAre(testing::Pair(
          testing::FieldsAre(kInode1, kCrtime1, base::FilePath(kFilePath1),
                             kExampleUrl1, kReferrerUrl1),
          ::dlp::RestrictionLevel::LEVEL_BLOCK))))
      .Times(2);

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .Times(2)
      .WillRepeatedly(
          testing::DoAll(testing::SetArgPointee<3>(kExampleSourceUrl1),
                         testing::SetArgPointee<5>(kRuleMetadata1),
                         testing::Return(DlpRulesManager::Level::kWarn)));

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/1234, {file}, DlpFileDestination(GURL(kFileManagerUrl)),
      dlp::FileAction::kTransfer, result_callback.Get());

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/1234, {file}, DlpFileDestination(GURL(kImageLoaderUrl)),
      dlp::FileAction::kTransfer, result_callback.Get());
}

TEST_F(DlpFilesControllerAshTest, IsFilesTransferRestricted_MyFiles) {
  const auto histogram_tester = base::HistogramTester();

  auto fileDaemonInfo1 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(), kExampleUrl1, kReferrerUrl1);
  auto fileDaemonInfo2 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode2, kCrtime2, base::FilePath(), kExampleUrl2, kReferrerUrl2);
  auto fileDaemonInfo3 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode3, kCrtime3, base::FilePath(), kExampleUrl3, kReferrerUrl3);

  std::vector<DlpFilesControllerAsh::FileDaemonInfo> transferred_files(
      {fileDaemonInfo1, fileDaemonInfo2, fileDaemonInfo3});
  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels =
      {{fileDaemonInfo1, ::dlp::RestrictionLevel::LEVEL_ALLOW},
       {fileDaemonInfo2, ::dlp::RestrictionLevel::LEVEL_ALLOW},
       {fileDaemonInfo3, ::dlp::RestrictionLevel::LEVEL_ALLOW}};

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(files_levels)).Times(1);

  EXPECT_CALL(*rules_manager_, IsRestrictedComponent).Times(0);

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination).Times(0);

  EXPECT_CALL(*rules_manager_, GetReportingManager()).Times(0);

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/1234, transferred_files, DlpFileDestination(),
      dlp::FileAction::kTransfer, cb.Get());

  ASSERT_EQ(events_.size(), 0u);
}

class DlpFilesExternalDestinationTest
    : public DlpFilesTestWithMounts,
      public ::testing::WithParamInterface<
          std::tuple<std::string, std::string, data_controls::Component>> {
 public:
  DlpFilesExternalDestinationTest(const DlpFilesExternalDestinationTest&) =
      delete;
  DlpFilesExternalDestinationTest& operator=(
      const DlpFilesExternalDestinationTest&) = delete;

 protected:
  DlpFilesExternalDestinationTest() = default;
  ~DlpFilesExternalDestinationTest() override = default;

  void SetUp() override {
    DlpFilesTestWithMounts::SetUp();
    MountExternalComponents();
  }
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesExternalDestinationTest,
    ::testing::Values(
        std::make_tuple("android_files",
                        "path/in/android/filename",
                        data_controls::Component::kArc),
        std::make_tuple("removable",
                        "MyUSB/path/in/removable/filename",
                        data_controls::Component::kUsb),
        std::make_tuple("crostini_test_termina_penguin",
                        "path/in/crostini/filename",
                        data_controls::Component::kCrostini),
        std::make_tuple("drivefs-84675c855b63e12f384d45f033826980",
                        "root/path/in/mydrive/filename",
                        data_controls::Component::kDrive)));

TEST_P(DlpFilesExternalDestinationTest, IsFilesTransferRestricted_Component) {
  auto [mount_name, path, expected_component] = GetParam();

  const auto histogram_tester = base::HistogramTester();

  auto fileDaemonInfo1 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode1, kCrtime1, base::FilePath(), kExampleUrl1, kReferrerUrl1);
  auto fileDaemonInfo2 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode2, kCrtime2, base::FilePath(), kExampleUrl2, kReferrerUrl2);
  auto fileDaemonInfo3 = DlpFilesControllerAsh::FileDaemonInfo(
      kInode3, kCrtime3, base::FilePath(), kExampleUrl3, kReferrerUrl3);

  std::vector<DlpFilesControllerAsh::FileDaemonInfo> transferred_files(
      {fileDaemonInfo1, fileDaemonInfo2, fileDaemonInfo3});
  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels =
      {{fileDaemonInfo1, ::dlp::RestrictionLevel::LEVEL_BLOCK},
       {fileDaemonInfo2, ::dlp::RestrictionLevel::LEVEL_ALLOW},
       {fileDaemonInfo3, ::dlp::RestrictionLevel::LEVEL_BLOCK}};

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(files_levels)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _, _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<4>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kBlock)))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl2),
                               testing::SetArgPointee<4>(kRuleMetadata2),
                               testing::Return(DlpRulesManager::Level::kAllow)))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleUrl3),
                         testing::SetArgPointee<4>(kRuleMetadata3),
                         testing::Return(DlpRulesManager::Level::kBlock)));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/1234, transferred_files,
      DlpFileDestination(expected_component), dlp::FileAction::kTransfer,
      cb.Get());

  ASSERT_EQ(events_.size(), 2u);
  EXPECT_THAT(
      events_[0],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kExampleUrl1, expected_component,
          DlpRulesManager::Restriction::kFiles, kRuleName1, kRuleId1,
          DlpRulesManager::Level::kBlock)));
  EXPECT_THAT(
      events_[1],
      data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
          kExampleUrl3, expected_component,
          DlpRulesManager::Restriction::kFiles, kRuleName3, kRuleId3,
          DlpRulesManager::Level::kBlock)));
}

TEST_P(DlpFilesExternalDestinationTest, FileDownloadBlocked) {
  auto [mount_name, path, expected_component] = GetParam();

  MockCheckIfDlpAllowedCallback cb;
  EXPECT_CALL(cb, Run(/*is_allowed=*/false)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _, _))
      .WillOnce(
          testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                         testing::SetArgPointee<4>(kRuleMetadata1),
                         testing::Return(DlpRulesManager::Level::kBlock)));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{dst_url.path()},
                          dlp::FileAction::kDownload));

  files_controller_->CheckIfDownloadAllowed(
      DlpFileDestination(GURL(kExampleUrl1)), dst_url.path(), cb.Get());

  ASSERT_EQ(events_.size(), 1u);

  auto event_builder = data_controls::DlpPolicyEventBuilder::Event(
      GURL(kExampleUrl1).spec(), kRuleName1, kRuleId1,
      DlpRulesManager::Restriction::kFiles, DlpRulesManager::Level::kBlock);

  event_builder->SetDestinationComponent(expected_component);
  event_builder->SetContentName(base::FilePath(path).BaseName().value());

  EXPECT_THAT(events_[0],
              data_controls::IsDlpPolicyEvent(event_builder->Create()));
}

TEST_P(DlpFilesExternalDestinationTest, FilePromptForDownload) {
  auto [mount_name, path, expected_component] = GetParam();

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, expected_component, _, _, _))
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock))
      .WillOnce(testing::Return(DlpRulesManager::Level::kWarn))
      .WillOnce(testing::Return(DlpRulesManager::Level::kReport));

  auto dst_url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  ASSERT_TRUE(dst_url.is_valid());

  // Block
  EXPECT_TRUE(files_controller_->ShouldPromptBeforeDownload(
      DlpFileDestination(GURL(kExampleUrl1)), dst_url.path()));
  // Warn
  EXPECT_TRUE(files_controller_->ShouldPromptBeforeDownload(
      DlpFileDestination(GURL(kExampleUrl1)), dst_url.path()));
  // Report
  EXPECT_FALSE(files_controller_->ShouldPromptBeforeDownload(
      DlpFileDestination(GURL(kExampleUrl1)), dst_url.path()));
}

class DlpFilesUrlDestinationTest
    : public DlpFilesControllerAshTest,
      public ::testing::WithParamInterface<
          std::tuple<std::vector<DlpRulesManager::Level>,
                     std::string>> {
 protected:
  std::vector<DlpFilesControllerAsh::FileDaemonInfo> transferred_files{
      DlpFilesControllerAsh::FileDaemonInfo(kInode1,
                                            kCrtime1,
                                            base::FilePath(),
                                            kExampleUrl1,
                                            kReferrerUrl1),
      DlpFilesControllerAsh::FileDaemonInfo(kInode2,
                                            kCrtime2,
                                            base::FilePath(),
                                            kExampleUrl2,
                                            kReferrerUrl2),
      DlpFilesControllerAsh::FileDaemonInfo(kInode3,
                                            kCrtime3,
                                            base::FilePath(),
                                            kExampleUrl3,
                                            kReferrerUrl3)};
  std::vector<std::string> rules_names = {kRuleName1, kRuleName2, kRuleName3};
  std::vector<std::string> rules_ids = {kRuleId1, kRuleId2, kRuleId3};
};
INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesUrlDestinationTest,
    ::testing::Values(
        std::make_tuple(
            std::vector<DlpRulesManager::Level>{DlpRulesManager::Level::kBlock,
                                                DlpRulesManager::Level::kAllow,
                                                DlpRulesManager::Level::kBlock},
            "https://wetransfer.com/"),
        std::make_tuple(
            std::vector<DlpRulesManager::Level>{DlpRulesManager::Level::kAllow,
                                                DlpRulesManager::Level::kAllow,
                                                DlpRulesManager::Level::kAllow},
            "https://drive.google.com/")));

TEST_P(DlpFilesUrlDestinationTest, IsFilesTransferRestricted_Url) {
  auto [levels, destination_url] = GetParam();
  ASSERT_EQ(levels.size(), transferred_files.size());

  const auto histogram_tester = base::HistogramTester();

  std::vector<std::pair<FileDaemonInfo, ::dlp::RestrictionLevel>> files_levels;
  std::vector<std::string> disallowed_source_urls;
  std::vector<std::string> triggered_rule_names;
  std::vector<std::string> triggered_rule_ids;
  for (size_t i = 0; i < transferred_files.size(); ++i) {
    if (levels[i] == DlpRulesManager::Level::kBlock) {
      files_levels.emplace_back(transferred_files[i],
                                ::dlp::RestrictionLevel::LEVEL_BLOCK);
      disallowed_source_urls.emplace_back(
          transferred_files[i].source_url.spec());
      triggered_rule_names.emplace_back(rules_names[i]);
      triggered_rule_ids.emplace_back(rules_ids[i]);
    } else {
      files_levels.emplace_back(transferred_files[i],
                                ::dlp::RestrictionLevel::LEVEL_ALLOW);
    }
  }
  EXPECT_CALL(*rules_manager_, IsRestrictedDestination(_, _, _, _, _, _))
      .WillOnce(testing::DoAll(
          testing::SetArgPointee<3>(transferred_files[0].source_url.spec()),
          testing::SetArgPointee<5>(kRuleMetadata1),
          testing::Return(levels[0])))
      .WillOnce(testing::DoAll(
          testing::SetArgPointee<3>(transferred_files[1].source_url.spec()),
          testing::SetArgPointee<5>(kRuleMetadata2),
          testing::Return(levels[1])))
      .WillOnce(testing::DoAll(
          testing::SetArgPointee<3>(transferred_files[2].source_url.spec()),
          testing::SetArgPointee<5>(kRuleMetadata3),
          testing::Return(levels[2])));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(files_levels)).Times(1);

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/std::nullopt, transferred_files,
      DlpFileDestination(GURL(destination_url)), dlp::FileAction::kDownload,
      cb.Get());

  ASSERT_EQ(events_.size(), disallowed_source_urls.size());
  for (size_t i = 0u; i < disallowed_source_urls.size(); ++i) {
    EXPECT_THAT(
        events_[i],
        data_controls::IsDlpPolicyEvent(data_controls::CreateDlpPolicyEvent(
            disallowed_source_urls[i], destination_url,
            DlpRulesManager::Restriction::kFiles, triggered_rule_names[i],
            triggered_rule_ids[i], DlpRulesManager::Level::kBlock)));
  }
}

class DlpFilesWarningDialogChoiceTest
    : public DlpFilesControllerAshTest,
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
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kRemovableMediaPath)));

  EXPECT_CALL(*fpnm_, ShowDlpWarning)
      .WillOnce([&choice_result](
                    WarningWithJustificationCallback callback,
                    std::optional<file_manager::io_task::IOTaskId> task_id,
                    const std::vector<base::FilePath>& warning_files,
                    const DlpFileDestination& destination,
                    dlp::FileAction action) {
        std::move(callback).Run(/*user_justification=*/std::nullopt,
                                choice_result);
        return nullptr;
      });

  MockCheckIfDlpAllowedCallback cb;
  EXPECT_CALL(cb, Run(/*is_allowed=*/choice_result)).Times(1);

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, data_controls::Component::kUsb, _, _, _))
      .WillOnce(testing::DoAll(testing::SetArgPointee<3>(kExampleUrl1),
                               testing::SetArgPointee<4>(kRuleMetadata1),
                               testing::Return(DlpRulesManager::Level::kWarn)));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  const base::FilePath file_path("MyUSB/path/in/removable/filename");

  auto dst_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey(), "removable", file_path);
  ASSERT_TRUE(dst_url.is_valid());

  if (!choice_result) {
    EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                            /*task_id=*/{std::nullopt},
                            std::vector<base::FilePath>{dst_url.path()},
                            dlp::FileAction::kDownload));
  }

  files_controller_->CheckIfDownloadAllowed(
      DlpFileDestination(GURL(kExampleUrl1)), dst_url.path(), cb.Get());

  auto CreateEvent =
      [&](std::optional<DlpRulesManager::Level> level) -> DlpPolicyEvent {
    auto event_builder =
        level.has_value()
            ? data_controls::DlpPolicyEventBuilder::Event(
                  GURL(kExampleUrl1).spec(), kRuleName1, kRuleId1,
                  DlpRulesManager::Restriction::kFiles, level.value())
            : data_controls::DlpPolicyEventBuilder::WarningProceededEvent(
                  GURL(kExampleUrl1).spec(), kRuleName1, kRuleId1,
                  DlpRulesManager::Restriction::kFiles);
    event_builder->SetDestinationComponent(data_controls::Component::kUsb);
    event_builder->SetContentName(file_path.BaseName().value());
    return event_builder->Create();
  };

  ASSERT_EQ(events_.size(), 1u + (choice_result ? 1 : 0));
  EXPECT_THAT(events_[0], data_controls::IsDlpPolicyEvent(
                              CreateEvent(DlpRulesManager::Level::kWarn)));
  if (choice_result) {
    EXPECT_THAT(events_[1],
                data_controls::IsDlpPolicyEvent(CreateEvent(std::nullopt)));
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          data_controls::GetDlpHistogramPrefix() +
          std::string(data_controls::dlp::kFileActionWarnProceededUMA)),
      base::BucketsAre(base::Bucket(dlp::FileAction::kDownload, choice_result),
                       base::Bucket(dlp::FileAction::kTransfer, 0)));

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

class DlpFilesWarningDialogContentTest
    : public DlpFilesControllerAshTest,
      public ::testing::WithParamInterface<FilesTransferInfo> {};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesWarningDialogContentTest,
    ::testing::Values(
        FilesTransferInfo(policy::dlp::FileAction::kDownload,
                          std::vector<ino_t>({kInode1}),
                          std::vector<time_t>({kCrtime1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::dlp::FileAction::kTransfer,
                          std::vector<ino_t>({kInode1}),
                          std::vector<time_t>({kCrtime1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::dlp::FileAction::kTransfer,
                          std::vector<ino_t>({kInode1, kInode2}),
                          std::vector<time_t>({kCrtime1, kCrtime2}),
                          std::vector<std::string>({kExampleUrl1,
                                                    kExampleUrl2}),
                          std::vector<std::string>({kFilePath1, kFilePath2})),
        FilesTransferInfo(policy::dlp::FileAction::kUpload,
                          std::vector<ino_t>({kInode1}),
                          std::vector<time_t>({kCrtime1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::dlp::FileAction::kUpload,
                          std::vector<ino_t>({kInode1, kInode2}),
                          std::vector<time_t>({kCrtime1, kCrtime2}),
                          std::vector<std::string>({kExampleUrl1,
                                                    kExampleUrl2}),
                          std::vector<std::string>({kFilePath1, kFilePath2})),
        FilesTransferInfo(policy::dlp::FileAction::kCopy,
                          std::vector<ino_t>({kInode1}),
                          std::vector<time_t>({kCrtime1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::dlp::FileAction::kCopy,
                          std::vector<ino_t>({kInode1, kInode2}),
                          std::vector<time_t>({kCrtime1, kCrtime2}),
                          std::vector<std::string>({kExampleUrl1,
                                                    kExampleUrl2}),
                          std::vector<std::string>({kFilePath1, kFilePath2})),
        FilesTransferInfo(policy::dlp::FileAction::kMove,
                          std::vector<ino_t>({kInode1}),
                          std::vector<time_t>({kCrtime1}),
                          std::vector<std::string>({kExampleUrl1}),
                          std::vector<std::string>({kFilePath1})),
        FilesTransferInfo(policy::dlp::FileAction::kMove,
                          std::vector<ino_t>({kInode1, kInode2}),
                          std::vector<time_t>({kCrtime1, kCrtime2}),
                          std::vector<std::string>({kExampleUrl1,
                                                    kExampleUrl2}),
                          std::vector<std::string>({kFilePath1, kFilePath2}))));

TEST_P(DlpFilesWarningDialogContentTest,
       IsFilesTransferRestricted_WarningDialogContent) {
  auto transfer_info = GetParam();
  std::vector<DlpFilesControllerAsh::FileDaemonInfo> warned_files;
  std::vector<
      std::pair<DlpFilesControllerAsh::FileDaemonInfo, ::dlp::RestrictionLevel>>
      files_levels;
  for (size_t i = 0; i < transfer_info.file_sources.size(); ++i) {
    DlpFilesControllerAsh::FileDaemonInfo file_info(
        transfer_info.file_inodes[i], transfer_info.file_crtimes[i],
        base::FilePath(transfer_info.file_paths[i]),
        transfer_info.file_sources[i], /*referrer_url=*/"");
    warned_files.emplace_back(file_info);
    files_levels.emplace_back(file_info,
                              ::dlp::RestrictionLevel::LEVEL_WARN_CANCEL);
  }
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  ASSERT_TRUE(mount_points);
  mount_points->RevokeAllFileSystems();
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      ash::kSystemMountNameRemovable, storage::kFileSystemTypeLocal,
      storage::FileSystemMountOption(),
      base::FilePath(file_manager::util::kRemovableMediaPath)));
  std::vector<FileDaemonInfo> files{
      FileDaemonInfo(kInode1, kCrtime1, my_files_dir_.AppendASCII(kFilePath1),
                     kExampleUrl1, kReferrerUrl1),
      FileDaemonInfo(kInode2, kCrtime2, my_files_dir_.AppendASCII(kFilePath2),
                     kExampleUrl2, kReferrerUrl2),
      FileDaemonInfo(kInode3, kCrtime3, my_files_dir_.AppendASCII(kFilePath3),
                     kExampleUrl3, kReferrerUrl3)};
  std::vector<FileSystemURL> files_urls;
  AddFilesToDlpClient(std::move(files), files_urls);

  std::vector<base::FilePath> expected_files;

  for (const auto& file_path : transfer_info.file_paths) {
    expected_files.emplace_back(base::FilePath(file_path));
  }

  EXPECT_CALL(*rules_manager_,
              IsRestrictedComponent(_, data_controls::Component::kUsb, _, _, _))
      .WillRepeatedly(testing::Return(DlpRulesManager::Level::kWarn));

  EXPECT_CALL(*rules_manager_, GetReportingManager())
      .Times(::testing::AnyNumber());

  EXPECT_CALL(*fpnm_,
              ShowDlpWarning(base::test::IsNotNullCallback(), {std::nullopt},
                             std::move(expected_files),
                             DlpFileDestination(data_controls::Component::kUsb),
                             transfer_info.files_action))
      .WillOnce([](WarningWithJustificationCallback callback,
                   std::optional<file_manager::io_task::IOTaskId> task_id,
                   const std::vector<base::FilePath>& confidential_files,
                   const DlpFileDestination& destination,
                   dlp::FileAction action) {
        std::move(callback).Run(/*user_justification=*/std::nullopt, false);
        return nullptr;
      });

  MockIsFilesTransferRestrictedCallback cb;
  EXPECT_CALL(cb, Run(files_levels)).Times(1);

  auto dst_url = mount_points->CreateExternalFileSystemURL(
      blink::StorageKey(), "removable",
      base::FilePath("MyUSB/path/in/removable"));
  ASSERT_TRUE(dst_url.is_valid());

  files_controller_->IsFilesTransferRestricted(
      /*task_id=*/std::nullopt, warned_files,
      DlpFileDestination(data_controls::Component::kUsb),
      transfer_info.files_action, cb.Get());

  storage::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
}

class DlpFilesAppServiceTest : public DlpFilesControllerAshTest {
 public:
  DlpFilesAppServiceTest(const DlpFilesAppServiceTest&) = delete;
  DlpFilesAppServiceTest& operator=(const DlpFilesAppServiceTest&) = delete;

 protected:
  DlpFilesAppServiceTest() = default;

  ~DlpFilesAppServiceTest() override = default;

  void SetUp() override {
    DlpFilesControllerAshTest::SetUp();
    app_service_test_.SetUp(profile_.get());
    app_service_proxy_ =
        apps::AppServiceProxyFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(app_service_proxy_);
  }

  void CreateAndStoreFakeApp(
      std::string fake_id,
      apps::AppType app_type,
      std::optional<std::string> publisher_id = std::nullopt) {
    std::vector<apps::AppPtr> fake_apps;
    apps::AppPtr fake_app = std::make_unique<apps::App>(app_type, fake_id);
    fake_app->name = "xyz";
    fake_app->show_in_management = true;
    fake_app->readiness = apps::Readiness::kReady;
    if (publisher_id.has_value()) {
      fake_app->publisher_id = publisher_id.value();
    }

    std::vector<apps::PermissionPtr> fake_permissions;
    fake_app->permissions = std::move(fake_permissions);

    fake_apps.push_back(std::move(fake_app));

    UpdateAppRegistryCache(std::move(fake_apps), app_type);
  }

  void UpdateAppRegistryCache(std::vector<apps::AppPtr> fake_apps,
                              apps::AppType app_type) {
    app_service_proxy_->OnApps(std::move(fake_apps), app_type,
                               /*should_notify_initialized=*/false);
  }

  raw_ptr<apps::AppServiceProxy, DanglingUntriaged> app_service_proxy_ =
      nullptr;
  apps::AppServiceTest app_service_test_;
};

TEST_F(DlpFilesAppServiceTest, CheckIfLaunchAllowed_ErrorResponse) {
  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.set_error_message("Did not receive a reply.");
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  CreateAndStoreFakeApp("arcApp", apps::AppType::kArc);

  auto app_service_intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView);
  app_service_intent->mime_type = "*/*";
  const std::string path = "Documents/foo.txt";
  const std::string mime_type = "text/plain";
  auto url = ToGURL(base::FilePath(storage::kTestDir), path);
  EXPECT_TRUE(url.SchemeIsFileSystem());
  app_service_intent->files = std::vector<apps::IntentFilePtr>{};
  auto file = std::make_unique<apps::IntentFile>(url);
  file->mime_type = mime_type;
  app_service_intent->files.push_back(std::move(file));
  EXPECT_FALSE(app_service_intent->IsShareIntent());

  base::test::TestFuture<bool> launch_cb;
  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      kArcAppId,
      [this, &app_service_intent, &launch_cb](const apps::AppUpdate& update) {
        files_controller_->CheckIfLaunchAllowed(
            update, std::move(app_service_intent), launch_cb.GetCallback());
      }));
  EXPECT_EQ(launch_cb.Get(), true);

  auto last_check_files_transfer_request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  EXPECT_TRUE(last_check_files_transfer_request.has_file_action());
  EXPECT_EQ(last_check_files_transfer_request.file_action(),
            ::dlp::FileAction::OPEN);
  ASSERT_EQ(last_check_files_transfer_request.files_paths().size(), 1);
  EXPECT_EQ(last_check_files_transfer_request.files_paths()[0], path);
  EXPECT_EQ(last_check_files_transfer_request.destination_component(),
            ::dlp::DlpComponent::ARC);
  EXPECT_FALSE(last_check_files_transfer_request.has_io_task_id());
}

TEST_F(DlpFilesAppServiceTest, CheckIfLaunchAllowed_EmptyIntent) {
  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.set_error_message("Did not receive a reply.");
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  CreateAndStoreFakeApp("arcApp", apps::AppType::kArc);

  auto app_service_intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView);

  base::test::TestFuture<bool> launch_cb;
  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      kArcAppId,
      [this, &app_service_intent, &launch_cb](const apps::AppUpdate& update) {
        files_controller_->CheckIfLaunchAllowed(
            update, std::move(app_service_intent), launch_cb.GetCallback());
      }));
  EXPECT_EQ(launch_cb.Get(), true);
}

TEST_F(DlpFilesAppServiceTest, IsLaunchBlocked_EmptyIntent) {
  CreateAndStoreFakeApp("arcApp", apps::AppType::kArc);

  ON_CALL(*rules_manager_, IsRestrictedComponent)
      .WillByDefault(testing::Return(DlpRulesManager::Level::kBlock));

  auto app_service_intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView);

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      kArcAppId, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_FALSE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

TEST_F(DlpFilesAppServiceTest, IsLaunchBlocked_NoSourceUrl) {
  CreateAndStoreFakeApp("arcApp", apps::AppType::kArc);

  ON_CALL(*rules_manager_, IsRestrictedComponent)
      .WillByDefault(testing::Return(DlpRulesManager::Level::kBlock));

  auto app_service_intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
  app_service_intent->mime_type = "*/*";
  app_service_intent->files = std::vector<apps::IntentFilePtr>{};
  auto url1 = ToGURL(base::FilePath(storage::kTestDir), "Documents/foo1.txt");
  EXPECT_TRUE(url1.SchemeIsFileSystem());
  auto file1 = std::make_unique<apps::IntentFile>(url1);
  file1->mime_type = "text/plain";
  app_service_intent->files.push_back(std::move(file1));

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      kArcAppId, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_FALSE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

class DlpFilesAppLaunchTest : public DlpFilesAppServiceTest {
 public:
  DlpFilesAppLaunchTest(const DlpFilesAppServiceTest&) = delete;
  DlpFilesAppLaunchTest& operator=(const DlpFilesAppServiceTest&) = delete;

 protected:
  DlpFilesAppLaunchTest() = default;

  ~DlpFilesAppLaunchTest() override = default;

  std::unique_ptr<apps::Intent> GetAppServiceIntent(
      const std::vector<std::string>& paths = {"Documents/foo1.txt",
                                               "Documents/foo2.txt"}) {
    auto app_service_intent =
        std::make_unique<apps::Intent>(apps_util::kIntentActionSend);
    app_service_intent->mime_type = "*/*";
    app_service_intent->files = std::vector<apps::IntentFilePtr>{};

    for (const auto& path : paths) {
      auto url = ToGURL(base::FilePath(storage::kTestDir), path);
      EXPECT_TRUE(url.SchemeIsFileSystem());
      auto file = std::make_unique<apps::IntentFile>(url);
      file->mime_type = "text/plain";
      file->dlp_source_url = kExampleUrl1;
      app_service_intent->files.push_back(std::move(file));
    }

    return app_service_intent;
  }

  const std::string path1 = "Documents/foo1.txt";
  const std::string path2 = "Documents/foo2.txt";
};

class DlpFilesAppLaunchTest_ExtensionApp
    : public DlpFilesAppLaunchTest,
      public ::testing::WithParamInterface<
          std::tuple<apps::AppType, std::string>> {
 protected:
  void SetUp() override {
    DlpFilesAppLaunchTest::SetUp();

    CreateAndStoreFakeApp(kStandaloneBrowserChromeAppId,
                          apps::AppType::kStandaloneBrowserChromeApp,
                          kExampleUrl1);
    CreateAndStoreFakeApp(kExtensionAppId, apps::AppType::kExtension,
                          kExampleUrl2);
    CreateAndStoreFakeApp(kStandaloneBrowserExtensionAppId,
                          apps::AppType::kStandaloneBrowserExtension,
                          kExampleUrl3);
    CreateAndStoreFakeApp(kChromeAppId, apps::AppType::kChromeApp,
                          kExampleUrl4);
  }
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesAppLaunchTest_ExtensionApp,
    ::testing::Values(
        std::make_tuple(apps::AppType::kStandaloneBrowserChromeApp,
                        kStandaloneBrowserChromeAppId),
        std::make_tuple(apps::AppType::kExtension, kExtensionAppId),
        std::make_tuple(apps::AppType::kStandaloneBrowserExtension,
                        kStandaloneBrowserExtensionAppId),
        std::make_tuple(apps::AppType::kChromeApp, kChromeAppId)));

TEST_P(DlpFilesAppLaunchTest_ExtensionApp, CheckIfAppLaunchAllowed) {
  auto [app_type, app_id] = GetParam();

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(path1);
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{base::FilePath(path1)},
                          dlp::FileAction::kOpen));

  auto app_service_intent = GetAppServiceIntent();
  EXPECT_TRUE(app_service_intent->IsShareIntent());

  base::test::TestFuture<bool> launch_cb;
  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id,
      [this, &app_service_intent, &launch_cb](const apps::AppUpdate& update) {
        files_controller_->CheckIfLaunchAllowed(
            update, std::move(app_service_intent), launch_cb.GetCallback());
      }));
  EXPECT_EQ(launch_cb.Get(), false);

  auto last_check_files_transfer_request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();

  EXPECT_TRUE(last_check_files_transfer_request.has_destination_url());
  EXPECT_EQ(GURL(last_check_files_transfer_request.destination_url()),
            GURL(std::string(extensions::kExtensionScheme) + "://" + app_id));

  EXPECT_TRUE(last_check_files_transfer_request.has_file_action());
  EXPECT_EQ(last_check_files_transfer_request.file_action(),
            ::dlp::FileAction::SHARE);
  EXPECT_FALSE(last_check_files_transfer_request.has_io_task_id());

  std::vector<std::string> expected_requested_files;
  expected_requested_files.push_back(path1);
  expected_requested_files.push_back(path2);
  std::vector<std::string> requested_files(
      last_check_files_transfer_request.files_paths().begin(),
      last_check_files_transfer_request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
}

TEST_P(DlpFilesAppLaunchTest_ExtensionApp, IsLaunchBlocked) {
  auto [app_type, app_id] = GetParam();

  auto app_service_intent = GetAppServiceIntent({path1});

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_TRUE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

class DlpFilesAppLaunchTest_WebApp
    : public DlpFilesAppLaunchTest,
      public ::testing::WithParamInterface<
          std::tuple<apps::AppType, std::string, std::string>> {
 protected:
  void SetUp() override {
    DlpFilesAppLaunchTest::SetUp();

    CreateAndStoreFakeApp(kWebAppId, apps::AppType::kWeb, kExampleUrl1);
    CreateAndStoreFakeApp(kSystemWebAppId, apps::AppType::kSystemWeb,
                          kExampleUrl2);
  }
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesAppLaunchTest_WebApp,
    ::testing::Values(
        std::make_tuple(apps::AppType::kWeb, kWebAppId, kExampleUrl1),
        std::make_tuple(apps::AppType::kSystemWeb,
                        kSystemWebAppId,
                        kExampleUrl2)));

TEST_P(DlpFilesAppLaunchTest_WebApp, CheckIfAppLaunchAllowed) {
  auto [app_type, app_id, url] = GetParam();

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(path1);
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{base::FilePath(path1)},
                          dlp::FileAction::kOpen));

  auto app_service_intent = GetAppServiceIntent();
  EXPECT_TRUE(app_service_intent->IsShareIntent());

  base::test::TestFuture<bool> launch_cb;
  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id,
      [this, &app_service_intent, &launch_cb](const apps::AppUpdate& update) {
        files_controller_->CheckIfLaunchAllowed(
            update, std::move(app_service_intent), launch_cb.GetCallback());
      }));
  EXPECT_EQ(launch_cb.Get(), false);

  auto last_check_files_transfer_request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();

  EXPECT_TRUE(last_check_files_transfer_request.has_destination_url());
  EXPECT_EQ(GURL(last_check_files_transfer_request.destination_url()),
            GURL(url));

  EXPECT_TRUE(last_check_files_transfer_request.has_file_action());
  EXPECT_EQ(last_check_files_transfer_request.file_action(),
            ::dlp::FileAction::SHARE);
  EXPECT_FALSE(last_check_files_transfer_request.has_io_task_id());

  std::vector<std::string> expected_requested_files;
  expected_requested_files.push_back(path1);
  expected_requested_files.push_back(path2);
  std::vector<std::string> requested_files(
      last_check_files_transfer_request.files_paths().begin(),
      last_check_files_transfer_request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
}

TEST_P(DlpFilesAppLaunchTest_WebApp, IsLaunchBlocked) {
  auto [app_type, app_id, url] = GetParam();

  auto app_service_intent = GetAppServiceIntent({path1});

  EXPECT_CALL(*rules_manager_, IsRestrictedDestination)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_TRUE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

TEST_P(DlpFilesAppLaunchTest_WebApp, IsLaunchBlocked_Empty) {
  auto [app_type, app_id, url] = GetParam();

  auto app_service_intent = GetAppServiceIntent({});

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_FALSE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

class DlpFilesAppLaunchTest_Component
    : public DlpFilesAppLaunchTest,
      public ::testing::WithParamInterface<
          std::tuple<apps::AppType, std::string, ::dlp::DlpComponent>> {
 protected:
  void SetUp() override {
    DlpFilesAppLaunchTest::SetUp();

    CreateAndStoreFakeApp(kArcAppId, apps::AppType::kArc, kExampleUrl1);
    CreateAndStoreFakeApp(kCrostiniAppId, apps::AppType::kCrostini,
                          kExampleUrl2);
    CreateAndStoreFakeApp(kPluginVmAppId, apps::AppType::kPluginVm,
                          kExampleUrl3);
  }
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesAppLaunchTest_Component,
    ::testing::Values(std::make_tuple(apps::AppType::kArc,
                                      kArcAppId,
                                      ::dlp::DlpComponent::ARC),
                      std::make_tuple(apps::AppType::kCrostini,
                                      kCrostiniAppId,
                                      ::dlp::DlpComponent::CROSTINI),
                      std::make_tuple(apps::AppType::kPluginVm,
                                      kPluginVmAppId,
                                      ::dlp::DlpComponent::PLUGIN_VM)));

TEST_P(DlpFilesAppLaunchTest_Component, CheckIfAppLaunchAllowed) {
  auto [app_type, app_id, expected_component] = GetParam();

  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(path1);
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{base::FilePath(path1)},
                          dlp::FileAction::kOpen));

  auto app_service_intent = GetAppServiceIntent();
  EXPECT_TRUE(app_service_intent->IsShareIntent());

  base::test::TestFuture<bool> launch_cb;
  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id,
      [this, &app_service_intent, &launch_cb](const apps::AppUpdate& update) {
        files_controller_->CheckIfLaunchAllowed(
            update, std::move(app_service_intent), launch_cb.GetCallback());
      }));
  EXPECT_EQ(launch_cb.Get(), false);

  auto last_check_files_transfer_request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();

  EXPECT_TRUE(last_check_files_transfer_request.has_destination_component());
  EXPECT_EQ(last_check_files_transfer_request.destination_component(),
            expected_component);

  EXPECT_TRUE(last_check_files_transfer_request.has_file_action());
  EXPECT_EQ(last_check_files_transfer_request.file_action(),
            ::dlp::FileAction::SHARE);
  EXPECT_FALSE(last_check_files_transfer_request.has_io_task_id());

  std::vector<std::string> expected_requested_files;
  expected_requested_files.push_back(path1);
  expected_requested_files.push_back(path2);
  std::vector<std::string> requested_files(
      last_check_files_transfer_request.files_paths().begin(),
      last_check_files_transfer_request.files_paths().end());
  EXPECT_THAT(requested_files,
              testing::UnorderedElementsAreArray(expected_requested_files));
}

TEST_P(DlpFilesAppLaunchTest_Component, IsLaunchBlocked) {
  auto [app_type, app_id, expected_component] = GetParam();

  auto app_service_intent = GetAppServiceIntent({path1});

  EXPECT_CALL(*rules_manager_, IsRestrictedComponent)
      .WillOnce(testing::Return(DlpRulesManager::Level::kBlock));

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_TRUE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

class DlpFilesAppLaunchTest_Unsupported
    : public DlpFilesAppLaunchTest,
      public ::testing::WithParamInterface<
          std::tuple<apps::AppType, std::string>> {
 protected:
  void SetUp() override {
    DlpFilesAppLaunchTest::SetUp();

    CreateAndStoreFakeApp(kUnknownAppId, apps::AppType::kUnknown, kExampleUrl1);
    CreateAndStoreFakeApp(kBuiltInAppId, apps::AppType::kBuiltIn, kExampleUrl2);
    CreateAndStoreFakeApp(kStandaloneBrowserAppId,
                          apps::AppType::kStandaloneBrowser, kExampleUrl4);
    CreateAndStoreFakeApp(kRemoteAppId, apps::AppType::kRemote, kExampleUrl5);
    CreateAndStoreFakeApp(kBorealisAppId, apps::AppType::kBorealis,
                          kExampleUrl6);
    CreateAndStoreFakeApp(kBruschettaAppId, apps::AppType::kBruschetta,
                          kExampleUrl7);
  }
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesAppLaunchTest_Unsupported,
    ::testing::Values(std::make_tuple(apps::AppType::kUnknown, kUnknownAppId),
                      std::make_tuple(apps::AppType::kBuiltIn, kBuiltInAppId),
                      std::make_tuple(apps::AppType::kStandaloneBrowser,
                                      kStandaloneBrowserAppId),
                      std::make_tuple(apps::AppType::kRemote, kRemoteAppId),
                      std::make_tuple(apps::AppType::kBorealis, kBorealisAppId),
                      std::make_tuple(apps::AppType::kBruschetta,
                                      kBruschettaAppId)));

TEST_P(DlpFilesAppLaunchTest_Unsupported, CheckIfAppLaunchAllowed) {
  auto [app_type, app_id] = GetParam();

  auto app_service_intent = GetAppServiceIntent();
  EXPECT_TRUE(app_service_intent->IsShareIntent());

  base::test::TestFuture<bool> launch_cb;
  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id,
      [this, &app_service_intent, &launch_cb](const apps::AppUpdate& update) {
        files_controller_->CheckIfLaunchAllowed(
            update, std::move(app_service_intent), launch_cb.GetCallback());
      }));
  EXPECT_TRUE(launch_cb.Get());
}

TEST_P(DlpFilesAppLaunchTest_Unsupported, IsLaunchBlocked) {
  auto [app_type, app_id] = GetParam();

  auto app_service_intent = GetAppServiceIntent({path1});

  ASSERT_TRUE(files_controller_);
  EXPECT_TRUE(app_service_proxy_->AppRegistryCache().ForOneApp(
      app_id, [this, &app_service_intent](const apps::AppUpdate& update) {
        EXPECT_FALSE(files_controller_->IsLaunchBlocked(
            update, std::move(app_service_intent)));
      }));
}

class DlpFilesDnDTest
    : public DlpFilesControllerAshTest,
      public ::testing::WithParamInterface<
          std::pair<ui::DataTransferEndpoint, ::dlp::DlpComponent>> {
 public:
  DlpFilesDnDTest(const DlpFilesDnDTest&) = delete;
  DlpFilesDnDTest& operator=(const DlpFilesDnDTest&) = delete;

 protected:
  DlpFilesDnDTest() = default;

  ~DlpFilesDnDTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesDnDTest,
    ::testing::Values(
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kArc),
                        ::dlp::DlpComponent::ARC),
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kCrostini),
                        ::dlp::DlpComponent::CROSTINI),
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kPluginVm),
                        ::dlp::DlpComponent::PLUGIN_VM),
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kLacros),
                        ::dlp::DlpComponent::UNKNOWN_COMPONENT),
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kDefault),
                        ::dlp::DlpComponent::UNKNOWN_COMPONENT),
        std::make_tuple(
            ui::DataTransferEndpoint(ui::EndpointType::kClipboardHistory),
            ::dlp::DlpComponent::UNKNOWN_COMPONENT),
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kBorealis),
                        ::dlp::DlpComponent::UNKNOWN_COMPONENT),
        std::make_tuple(ui::DataTransferEndpoint(ui::EndpointType::kUnknownVm),
                        ::dlp::DlpComponent::UNKNOWN_COMPONENT)));

// Tests dropping a mix of an external file and a local directory.
TEST_P(DlpFilesDnDTest, CheckIfDropAllowed) {
  storage::ExternalMountPoints* mount_points =
      storage::ExternalMountPoints::GetSystemInstance();
  ASSERT_TRUE(mount_points);
  ASSERT_TRUE(mount_points->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));
  absl::Cleanup external_mount_points_revoker = [mount_points] {
    mount_points->RevokeAllFileSystems();
  };

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(my_files_dir_));
  base::FilePath file_path1 = sub_dir1.GetPath().AppendASCII(kFilePath1);
  ASSERT_TRUE(CreateDummyFile(file_path1));
  auto file_url1 = CreateFileSystemURL(kTestStorageKey, file_path1.value());

  // Set CheckFilesTransfer response to restrict the local file.
  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(file_path1.value());
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{file_path1},
                          dlp::FileAction::kCopy));

  auto [data_dst, expected_component] = GetParam();

  base::test::TestFuture<bool> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfPasteOrDropIsAllowed({file_path1}, &data_dst,
                                                 future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(false, future.Take());

  // Validate that only the local file was sent to the daemon.
  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  ASSERT_EQ(request.files_paths().size(), 1);
  EXPECT_EQ(request.files_paths()[0], file_path1.value());
  ASSERT_TRUE(request.has_destination_component());
  EXPECT_EQ(request.destination_component(), expected_component);
  EXPECT_FALSE(request.has_io_task_id());
}

class DlpFilesControllerAshComponentsTest
    : public DlpFilesTestWithMounts,
      public ::testing::WithParamInterface<
          std::tuple<std::string,
                     std::string,
                     std::optional<data_controls::Component>>> {
 public:
  DlpFilesControllerAshComponentsTest(
      const DlpFilesControllerAshComponentsTest&) = delete;
  DlpFilesControllerAshComponentsTest& operator=(
      const DlpFilesControllerAshComponentsTest&) = delete;

 protected:
  DlpFilesControllerAshComponentsTest() = default;
  ~DlpFilesControllerAshComponentsTest() = default;

  void SetUp() override {
    DlpFilesTestWithMounts::SetUp();
    MountExternalComponents();
  }
};

INSTANTIATE_TEST_SUITE_P(
    DlpFiles,
    DlpFilesControllerAshComponentsTest,
    ::testing::Values(
        std::make_tuple("android_files",
                        "path/in/android",
                        data_controls::Component::kArc),
        std::make_tuple("removable",
                        "MyUSB/path/in/removable",
                        data_controls::Component::kUsb),
        std::make_tuple("crostini_test_termina_penguin",
                        "path/in/crostini",
                        data_controls::Component::kCrostini),
        std::make_tuple("drivefs-84675c855b63e12f384d45f033826980",
                        "root/path/in/mydrive",
                        data_controls::Component::kDrive),
        std::make_tuple("", "/Downloads", std::nullopt)));

TEST_P(DlpFilesControllerAshComponentsTest, MapFilePathToPolicyComponentTest) {
  auto [mount_name, path, expected_component] = GetParam();
  auto url = mount_points_->CreateExternalFileSystemURL(
      blink::StorageKey(), mount_name, base::FilePath(path));
  EXPECT_EQ(files_controller_->MapFilePathToPolicyComponent(profile_.get(),
                                                            url.path()),
            expected_component);
}

class DlpFilesControllerAshBlockUITest
    : public DlpFilesTestWithMounts,
      public ::testing::WithParamInterface<dlp::FileAction> {
 public:
  DlpFilesControllerAshBlockUITest(const DlpFilesControllerAshBlockUITest&) =
      delete;
  DlpFilesControllerAshBlockUITest& operator=(
      const DlpFilesControllerAshBlockUITest&) = delete;

 protected:
  DlpFilesControllerAshBlockUITest() = default;
  ~DlpFilesControllerAshBlockUITest() = default;

  void SetUp() override { DlpFilesTestWithMounts::SetUp(); }
};

INSTANTIATE_TEST_SUITE_P(DlpBlockUI,
                         DlpFilesControllerAshBlockUITest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kTransfer,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare));

TEST_P(DlpFilesControllerAshBlockUITest, ShowDlpBlockedFiles) {
  auto action = GetParam();

  base::FilePath path(kFilePath1);

  EXPECT_CALL(*fpnm_, ShowDlpBlockedFiles(
                          /*task_id=*/{std::nullopt},
                          std::vector<base::FilePath>{path}, action));

  files_controller_->ShowDlpBlockedFiles(
      /*task_id=*/std::nullopt, {path}, action);
}

}  // namespace policy
