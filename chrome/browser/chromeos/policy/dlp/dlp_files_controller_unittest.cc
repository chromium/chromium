// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "base/test/test_mock_time_task_runner.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_rules_manager.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths_lacros.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/reporting/util/test_util.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#endif

namespace policy {

namespace {

absl::optional<ino64_t> GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0) {
    return absl::nullopt;
  }
  return file_stats.st_ino;
}

using FileDaemonInfo = policy::DlpFilesController::FileDaemonInfo;
}  // namespace

class MockDlpFilesController : public DlpFilesController {
 public:
  explicit MockDlpFilesController(const DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  MOCK_METHOD(absl::optional<data_controls::Component>,
              MapFilePathtoPolicyComponent,
              (Profile * profile, const base::FilePath& file_path),
              (override));
};

class DlpFilesControllerTest : public testing::Test {
 public:
  DlpFilesControllerTest(const DlpFilesControllerTest&) = delete;
  DlpFilesControllerTest& operator=(const DlpFilesControllerTest&) = delete;

 protected:
  DlpFilesControllerTest() = default;
  ~DlpFilesControllerTest() override = default;

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    scoped_profile_ = std::make_unique<TestingProfile>();
    profile_ = scoped_profile_.get();
    AccountId account_id =
        AccountId::FromUserEmailGaiaId("test@example.com", "12345");
    profile_->SetIsNewProfile(true);
    user_manager::User* user =
        user_manager_->AddUserWithAffiliationAndTypeAndProfile(
            account_id, /*is_affiliated=*/false,
            user_manager::USER_TYPE_REGULAR, profile_);
    user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                /*browser_restart=*/false,
                                /*is_child=*/false);
    user_manager_->SimulateUserProfileLoad(account_id);
#else
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile("user", true);
#endif

    policy::DlpRulesManagerFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindRepeating(&DlpFilesControllerTest::SetDlpRulesManager,
                            base::Unretained(this)));

    ASSERT_TRUE(policy::DlpRulesManagerFactory::GetForPrimaryProfile());
    ASSERT_TRUE(rules_manager_);

    chromeos::DlpClient::InitializeFake();
    chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);

    base::PathService::Get(base::DIR_HOME, &my_files_dir_);
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    my_files_dir_url_ = CreateFileSystemURL(my_files_dir_.value());

    ASSERT_TRUE(files_controller_);
  }

  void TearDown() override {
    if (chromeos::DlpClient::Get()) {
      chromeos::DlpClient::Shutdown();
    }
  }

  std::unique_ptr<KeyedService> SetDlpRulesManager(
      content::BrowserContext* context) {
    auto dlp_rules_manager = std::make_unique<MockDlpRulesManager>();
    rules_manager_ = dlp_rules_manager.get();

    files_controller_ =
        std::make_unique<MockDlpFilesController>(*rules_manager_);

    task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    return dlp_rules_manager;
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal,
        base::FilePath::FromUTF8Unsafe(path));
  }

  content::BrowserTaskEnvironment task_environment_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestingProfile> scoped_profile_;
  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> user_manager_{
      new ash::FakeChromeUserManager()};
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_{
      std::make_unique<user_manager::ScopedUserManager>(
          base::WrapUnique(user_manager_.get()))};
#else
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
#endif
  raw_ptr<TestingProfile> profile_;

  raw_ptr<MockDlpRulesManager, ExperimentalAsh> rules_manager_ = nullptr;
  std::unique_ptr<MockDlpFilesController> files_controller_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  const blink::StorageKey kTestStorageKey =
      blink::StorageKey::CreateFromStringForTesting("https://example.com/test");
  base::FilePath my_files_dir_;
  storage::FileSystemURL my_files_dir_url_;
};

TEST_F(DlpFilesControllerTest, LocalFileCopyTest) {
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("test"));
  base::File(src_file, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE)
      .Flush();

  base::FilePath dest_file = my_files_dir_.Append(FILE_PATH_LITERAL("dest"));

  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);

  auto destination = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, dest_file);

  base::MockRepeatingCallback<void(
      const ::dlp::GetFilesSourcesRequest,
      chromeos::DlpClient::GetFilesSourcesCallback)>
      get_files_source_call;

  absl::optional<ino64_t> inode = GetInodeValue(src_file);
  EXPECT_TRUE(inode.has_value());

  ::dlp::GetFilesSourcesResponse response;
  auto* metadata = response.add_files_metadata();
  metadata->set_source_url("http://some.url/path");
  metadata->set_inode(inode.value());

  ::dlp::GetFilesSourcesRequest request;
  request.add_files_inodes(inode.value());

  EXPECT_CALL(get_files_source_call,
              Run(EqualsProto(request), base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(response));

  chromeos::DlpClient::Get()->GetTestInterface()->SetGetFilesSourceMock(
      get_files_source_call.Get());

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest,
      chromeos::DlpClient::RequestFileAccessCallback)>
      request_file_access_call;

  ::dlp::RequestFileAccessResponse access_response;
  access_response.set_allowed(true);
  EXPECT_CALL(
      request_file_access_call,
      Run(testing::Property(&::dlp::RequestFileAccessRequest::destination_url,
                            my_files_dir_.value()),
          base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));
  chromeos::DlpClient::Get()->GetTestInterface()->SetRequestFileAccessMock(
      request_file_access_call.Get());

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>>
      file_access_future;
  ASSERT_TRUE(files_controller_);
  EXPECT_CALL(*files_controller_, MapFilePathtoPolicyComponent)
      .WillOnce(testing::Return(absl::nullopt))
      .WillOnce(testing::Return(absl::nullopt));
  files_controller_->RequestCopyAccess(source, destination,
                                       file_access_future.GetCallback());
  std::unique_ptr<file_access::ScopedFileAccess> file_access =
      file_access_future.Take();
  EXPECT_TRUE(file_access->is_allowed());

  base::RunLoop run_loop;
  base::MockRepeatingCallback<void(
      const ::dlp::AddFilesRequest request,
      chromeos::DlpClient::AddFilesCallback callback)>
      add_files_call;

  ::dlp::AddFilesRequest expected_request;
  ::dlp::AddFileRequest* file_request =
      expected_request.add_add_file_requests();
  file_request->set_file_path(destination.path().value());
  file_request->set_source_url(metadata->source_url());

  EXPECT_CALL(add_files_call, Run(EqualsProto(expected_request),
                                  base::test::IsNotNullCallback()))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));

  chromeos::DlpClient::Get()->GetTestInterface()->SetAddFilesMock(
      add_files_call.Get());
  file_access.reset();
  run_loop.Run();
}

TEST_F(DlpFilesControllerTest, CopyEmptyMetadataTest) {
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("test"));
  base::File(src_file, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE)
      .Flush();

  base::FilePath dest_file = my_files_dir_.Append(FILE_PATH_LITERAL("dest"));

  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);

  auto destination = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, dest_file);

  base::MockRepeatingCallback<void(
      const ::dlp::GetFilesSourcesRequest,
      chromeos::DlpClient::GetFilesSourcesCallback)>
      get_files_source_call;

  absl::optional<ino64_t> inode = GetInodeValue(src_file);
  EXPECT_TRUE(inode.has_value());
  ::dlp::GetFilesSourcesResponse response;
  auto* metadata = response.add_files_metadata();
  metadata->set_source_url("");
  metadata->set_inode(inode.value());

  ::dlp::GetFilesSourcesRequest request;
  request.add_files_inodes(inode.value());

  EXPECT_CALL(get_files_source_call,
              Run(EqualsProto(request), base::test::IsNotNullCallback()))
      .WillOnce(base::test::RunOnceCallback<1>(response));

  chromeos::DlpClient::Get()->GetTestInterface()->SetGetFilesSourceMock(
      get_files_source_call.Get());

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest,
      chromeos::DlpClient::RequestFileAccessCallback)>
      request_file_access_call;

  EXPECT_CALL(request_file_access_call, Run).Times(0);
  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>>
      file_access_future;

  ASSERT_TRUE(files_controller_);
  files_controller_->RequestCopyAccess(source, destination,
                                       file_access_future.GetCallback());
  EXPECT_TRUE(file_access_future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, CopyNoClientTest) {
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("test"));
  base::File(src_file, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE)
      .Flush();

  base::FilePath dest_file = my_files_dir_.Append(FILE_PATH_LITERAL("dest"));

  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);

  auto destination = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, dest_file);

  ::chromeos::DlpClient::Shutdown();

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>>
      file_access_future;

  files_controller_->RequestCopyAccess(source, destination,
                                       file_access_future.GetCallback());
  EXPECT_TRUE(file_access_future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, FileCopyFromExternalTest) {
  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;

  EXPECT_CALL(request_file_access_call, Run).Times(0);

  EXPECT_CALL(*files_controller_, MapFilePathtoPolicyComponent)
      .WillOnce(testing::Return(absl::nullopt))
      .WillOnce(testing::Return(data_controls::Component::kDrive));

  chromeos::DlpClient::Get()->GetTestInterface()->SetRequestFileAccessMock(
      request_file_access_call.Get());

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->RequestCopyAccess(
      storage::FileSystemURL(), storage::FileSystemURL(), future.GetCallback());
  EXPECT_TRUE(future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, FileCopyToExternalAllowTest) {
  base::FilePath dest_file = my_files_dir_.Append(FILE_PATH_LITERAL("dest"));
  auto destination = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, dest_file);

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;

  ::dlp::RequestFileAccessResponse access_response;
  access_response.set_allowed(true);
  EXPECT_CALL(
      request_file_access_call,
      Run(testing::Property(&::dlp::RequestFileAccessRequest::destination_url,
                            destination.path().DirName().value()),
          base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));

  EXPECT_CALL(*files_controller_, MapFilePathtoPolicyComponent)
      .WillOnce(testing::Return(data_controls::Component::kDrive))
      .WillOnce(testing::Return(absl::nullopt));

  chromeos::DlpClient::Get()->GetTestInterface()->SetRequestFileAccessMock(
      request_file_access_call.Get());

  base::MockRepeatingCallback<void(
      const ::dlp::AddFilesRequest request,
      chromeos::DlpClient::AddFilesCallback callback)>
      add_files_call;
  EXPECT_CALL(add_files_call, Run).Times(0);
  chromeos::DlpClient::Get()->GetTestInterface()->SetAddFilesMock(
      add_files_call.Get());

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->RequestCopyAccess(storage::FileSystemURL(), destination,
                                       future.GetCallback());
  EXPECT_TRUE(future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, FileCopyToExternalDenyTest) {
  base::FilePath dest_file = my_files_dir_.Append(FILE_PATH_LITERAL("dest"));
  auto destination = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, dest_file);

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;

  ::dlp::RequestFileAccessResponse access_response;
  access_response.set_allowed(false);
  EXPECT_CALL(
      request_file_access_call,
      Run(testing::Property(&::dlp::RequestFileAccessRequest::destination_url,
                            destination.path().DirName().value()),
          base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));

  EXPECT_CALL(*files_controller_, MapFilePathtoPolicyComponent)
      .WillOnce(testing::Return(data_controls::Component::kDrive))
      .WillOnce(testing::Return(absl::nullopt));

  chromeos::DlpClient::Get()->GetTestInterface()->SetRequestFileAccessMock(
      request_file_access_call.Get());

  base::MockRepeatingCallback<void(
      const ::dlp::AddFilesRequest request,
      chromeos::DlpClient::AddFilesCallback callback)>
      add_files_call;
  EXPECT_CALL(add_files_call, Run).Times(0);
  chromeos::DlpClient::Get()->GetTestInterface()->SetAddFilesMock(
      add_files_call.Get());

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->RequestCopyAccess(storage::FileSystemURL(), destination,
                                       future.GetCallback());
  EXPECT_FALSE(future.Get()->is_allowed());
}

}  // namespace policy
