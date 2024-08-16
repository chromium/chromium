// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/enterprise/data_controls/core/browser/component.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;

namespace policy {

namespace {
using FileDaemonInfo = policy::DlpFilesController::FileDaemonInfo;

constexpr char kExampleUrl1[] = "https://1.example.com/";

constexpr char kFilePath1[] = "test1.txt";
constexpr char kFilePath2[] = "test2.txt";

bool CreateDummyFile(const base::FilePath& path) {
  return WriteFile(path, "42");
}
}  // namespace

class MockDlpFilesController : public DlpFilesController {
 public:
  explicit MockDlpFilesController(const DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  MOCK_METHOD(std::optional<data_controls::Component>,
              MapFilePathToPolicyComponent,
              (Profile * profile, const base::FilePath& file_path),
              (override));

  MOCK_METHOD(bool,
              IsInLocalFileSystem,
              (const base::FilePath& file_path),
              (override));

  MOCK_METHOD(void,
              ShowDlpBlockedFiles,
              (std::optional<uint64_t> task_id,
               std::vector<base::FilePath> blocked_files,
               dlp::FileAction action),
              (override));
};

class DlpFilesControllerTest : public DlpFilesTestBase {
 public:
  DlpFilesControllerTest(const DlpFilesControllerTest&) = delete;
  DlpFilesControllerTest& operator=(const DlpFilesControllerTest&) = delete;

 protected:
  DlpFilesControllerTest() = default;
  ~DlpFilesControllerTest() override = default;

  void SetUp() override {
    DlpFilesTestBase::SetUp();

    ASSERT_TRUE(rules_manager_);
    files_controller_ =
        std::make_unique<MockDlpFilesController>(*rules_manager_);

    chromeos::DlpClient::InitializeFake();
    chromeos::DlpClient::Get()->GetTestInterface()->SetIsAlive(true);

    base::PathService::Get(chrome::DIR_USER_DOCUMENTS, &my_files_dir_);
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    my_files_dir_url_ = CreateFileSystemURL(my_files_dir_.value());

    ASSERT_TRUE(files_controller_);

    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    ASSERT_TRUE(mount_points_);
    mount_points_->RevokeAllFileSystems();
  }

  void TearDown() override {
    DlpFilesTestBase::TearDown();

    if (chromeos::DlpClient::Get()) {
      chromeos::DlpClient::Shutdown();
    }
  }

  storage::FileSystemURL CreateFileSystemURL(const std::string& path) {
    return storage::FileSystemURL::CreateForTest(
        kTestStorageKey, storage::kFileSystemTypeLocal,
        base::FilePath::FromUTF8Unsafe(path));
  }

  std::unique_ptr<MockDlpFilesController> files_controller_;

  raw_ptr<storage::ExternalMountPoints> mount_points_ = nullptr;

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

  ::dlp::GetFilesSourcesResponse response;
  auto* metadata = response.add_files_metadata();
  metadata->set_source_url("http://some.url/path");
  metadata->set_path(src_file.value());

  ::dlp::GetFilesSourcesRequest request;
  request.add_files_paths(src_file.value());

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
      Run(testing::AllOf(
              testing::Property(
                  &::dlp::RequestFileAccessRequest::destination_component,
                  ::dlp::DlpComponent::SYSTEM),
              testing::Property(&::dlp::RequestFileAccessRequest::process_id,
                                base::GetCurrentProcId()),
              testing::Property(&::dlp::RequestFileAccessRequest::files_paths,
                                testing::ElementsAre(src_file.value()))),
          base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));
  chromeos::DlpClient::Get()->GetTestInterface()->SetRequestFileAccessMock(
      request_file_access_call.Get());

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>>
      file_access_future;
  ASSERT_TRUE(files_controller_);
  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
      .WillOnce(testing::Return(std::nullopt))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(true));
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
  file_request->set_referrer_url(metadata->referrer_url());

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

  ::dlp::GetFilesSourcesResponse response;
  auto* metadata = response.add_files_metadata();
  metadata->set_source_url("");
  metadata->set_path(src_file.value());

  ::dlp::GetFilesSourcesRequest request;
  request.add_files_paths(src_file.value());

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
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(true));
  files_controller_->RequestCopyAccess(source, destination,
                                       file_access_future.GetCallback());
  EXPECT_TRUE(file_access_future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, CopyNoMetadataTest) {
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

  ::dlp::GetFilesSourcesResponse response;

  ::dlp::GetFilesSourcesRequest request;
  request.add_files_paths(src_file.value());

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
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(true));
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

  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
      .WillOnce(testing::Return(std::nullopt))
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
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("source"));
  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;

  ::dlp::RequestFileAccessResponse access_response;
  access_response.set_allowed(true);
  EXPECT_CALL(
      request_file_access_call,
      Run(testing::AllOf(
              testing::Property(
                  &::dlp::RequestFileAccessRequest::destination_component,
                  ::dlp::DlpComponent::GOOGLE_DRIVE),
              testing::Property(&::dlp::RequestFileAccessRequest::process_id,
                                base::GetCurrentProcId()),
              testing::Property(&::dlp::RequestFileAccessRequest::files_paths,
                                testing::ElementsAre(src_file.value()))),
          base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));

  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
      .WillOnce(testing::Return(data_controls::Component::kDrive))
      .WillOnce(testing::Return(std::nullopt));

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
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true));
  files_controller_->RequestCopyAccess(source, storage::FileSystemURL(),
                                       future.GetCallback());
  EXPECT_TRUE(future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, FileCopyToExternalDenyTest) {
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("source"));
  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;

  ::dlp::RequestFileAccessResponse access_response;
  access_response.set_allowed(false);
  EXPECT_CALL(
      request_file_access_call,
      Run(testing::AllOf(
              testing::Property(
                  &::dlp::RequestFileAccessRequest::destination_component,
                  ::dlp::DlpComponent::GOOGLE_DRIVE),
              testing::Property(&::dlp::RequestFileAccessRequest::process_id,
                                base::GetCurrentProcId()),
              testing::Property(&::dlp::RequestFileAccessRequest::files_paths,
                                testing::ElementsAre(src_file.value()))),
          base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));

  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
      .WillOnce(testing::Return(data_controls::Component::kDrive))
      .WillOnce(testing::Return(std::nullopt));

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
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true));
  files_controller_->RequestCopyAccess(source, storage::FileSystemURL(),
                                       future.GetCallback());
  EXPECT_FALSE(future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, FileCopyToUnknownComponent) {
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("source"));
  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);
  auto destination = storage::FileSystemURL();

  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
      .WillOnce(testing::Return(std::nullopt))   // destination component
      .WillOnce(testing::Return(std::nullopt));  // source component

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;
  EXPECT_CALL(request_file_access_call, Run).Times(0);
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
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));
  files_controller_->RequestCopyAccess(source, destination,
                                       future.GetCallback());
  EXPECT_FALSE(future.Get()->is_allowed());
}

TEST_F(DlpFilesControllerTest, CheckIfPasteOrDropIsAllowed_ErrorResponse) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  base::FilePath file_path1 = my_files_dir_.AppendASCII(kFilePath1);
  ASSERT_TRUE(CreateDummyFile(file_path1));

  // Set CheckFilesTransferResponse to return an error.
  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(file_path1.value());
  check_files_transfer_response.set_error_message("Did not receive a reply.");
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  const ui::DataTransferEndpoint data_dst((GURL(kExampleUrl1)));

  base::test::TestFuture<bool> future;
  ASSERT_TRUE(files_controller_);
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true));

  files_controller_->CheckIfPasteOrDropIsAllowed({file_path1}, &data_dst,
                                                 future.GetCallback());

  ASSERT_TRUE(future.Take());

  // Validate the request sent to the daemon.
  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  ASSERT_EQ(request.files_paths().size(), 1);
  EXPECT_EQ(request.files_paths()[0], file_path1.value());
  EXPECT_EQ(kExampleUrl1, request.destination_url());
  EXPECT_EQ(::dlp::FileAction::COPY, request.file_action());
  EXPECT_FALSE(request.has_io_task_id());
}

// Tests pasting or dropping a mix of an external file and a local directory.
TEST_F(DlpFilesControllerTest, CheckIfPasteOrDropIsAllowed) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  base::ScopedTempDir external_dir;
  ASSERT_TRUE(external_dir.CreateUniqueTempDir());
  base::FilePath file_path1 = external_dir.GetPath().AppendASCII(kFilePath1);
  ASSERT_TRUE(CreateDummyFile(file_path1));
  auto file_url1 = CreateFileSystemURL(file_path1.value());

  base::ScopedTempDir sub_dir1;
  ASSERT_TRUE(sub_dir1.CreateUniqueTempDirUnderPath(my_files_dir_));
  base::FilePath file_path2 = sub_dir1.GetPath().AppendASCII(kFilePath2);
  ASSERT_TRUE(CreateDummyFile(file_path2));
  auto file_url2 = CreateFileSystemURL(file_path2.value());

  // Set CheckFilesTransfer response to restrict the local file.
  ::dlp::CheckFilesTransferResponse check_files_transfer_response;
  check_files_transfer_response.add_files_paths(file_path2.value());
  ASSERT_TRUE(chromeos::DlpClient::Get()->IsAlive());
  chromeos::DlpClient::Get()->GetTestInterface()->SetCheckFilesTransferResponse(
      check_files_transfer_response);

  EXPECT_CALL(*files_controller_,
              ShowDlpBlockedFiles(/*task_id=*/{std::nullopt},
                                  std::vector<base::FilePath>{file_path2},
                                  dlp::FileAction::kCopy));
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(false))
      .WillOnce(testing::Return(true));

  const ui::DataTransferEndpoint data_dst((GURL(kExampleUrl1)));

  base::test::TestFuture<bool> future;
  ASSERT_TRUE(files_controller_);
  files_controller_->CheckIfPasteOrDropIsAllowed(
      {file_path1, sub_dir1.GetPath()}, &data_dst, future.GetCallback());
  EXPECT_FALSE(future.Take());

  // Validate that only the local file was sent to the daemon.
  ::dlp::CheckFilesTransferRequest request =
      chromeos::DlpClient::Get()
          ->GetTestInterface()
          ->GetLastCheckFilesTransferRequest();
  ASSERT_EQ(request.files_paths().size(), 1);
  EXPECT_EQ(request.files_paths()[0], file_path2.value());
  EXPECT_EQ(kExampleUrl1, request.destination_url());
  EXPECT_EQ(::dlp::FileAction::COPY, request.file_action());
  EXPECT_FALSE(request.has_io_task_id());
}

TEST_F(DlpFilesControllerTest,
       CheckIfPasteOrDropIsAllowed_NoFileSystemContext) {
  ASSERT_TRUE(mount_points_->RegisterFileSystem(
      "c", storage::kFileSystemTypeLocal, storage::FileSystemMountOption(),
      my_files_dir_));

  base::FilePath file_path1 = my_files_dir_.AppendASCII(kFilePath1);
  ASSERT_TRUE(CreateDummyFile(file_path1));
  auto file_url1 = CreateFileSystemURL(file_path1.value());

  const ui::DataTransferEndpoint data_dst((GURL(kExampleUrl1)));

  base::test::TestFuture<bool> future;
  ASSERT_TRUE(files_controller_);
  EXPECT_CALL(*files_controller_, IsInLocalFileSystem)
      .WillOnce(testing::Return(true));
  files_controller_->SetFileSystemContextForTesting(nullptr);
  files_controller_->CheckIfPasteOrDropIsAllowed({file_path1}, &data_dst,
                                                 future.GetCallback());
  ASSERT_TRUE(future.Take());
}

}  // namespace policy
