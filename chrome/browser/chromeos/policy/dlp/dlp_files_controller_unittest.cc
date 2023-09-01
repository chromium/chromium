// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/test_future.h"
#include "chrome/browser/chromeos/policy/dlp/test/dlp_files_test_base.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "components/reporting/util/test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
using FileDaemonInfo = policy::DlpFilesController::FileDaemonInfo;
}  // namespace

class MockDlpFilesController : public DlpFilesController {
 public:
  explicit MockDlpFilesController(const DlpRulesManager& rules_manager)
      : DlpFilesController(rules_manager) {}
  MOCK_METHOD(absl::optional<data_controls::Component>,
              MapFilePathToPolicyComponent,
              (Profile * profile, const base::FilePath& file_path),
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

    base::PathService::Get(base::DIR_HOME, &my_files_dir_);
    ASSERT_TRUE(base::CreateDirectory(my_files_dir_));
    my_files_dir_url_ = CreateFileSystemURL(my_files_dir_.value());

    ASSERT_TRUE(files_controller_);
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
  EXPECT_CALL(request_file_access_call,
              Run(testing::Property(
                      &::dlp::RequestFileAccessRequest::destination_component,
                      ::dlp::DlpComponent::SYSTEM),
                  base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));
  chromeos::DlpClient::Get()->GetTestInterface()->SetRequestFileAccessMock(
      request_file_access_call.Get());

  base::test::TestFuture<std::unique_ptr<file_access::ScopedFileAccess>>
      file_access_future;
  ASSERT_TRUE(files_controller_);
  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
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
  base::FilePath src_file = my_files_dir_.Append(FILE_PATH_LITERAL("source"));
  auto source = storage::FileSystemURL::CreateForTest(
      kTestStorageKey, storage::kFileSystemTypeLocal, src_file);

  base::MockRepeatingCallback<void(
      ::dlp::RequestFileAccessRequest request,
      chromeos::DlpClient::RequestFileAccessCallback callback)>
      request_file_access_call;

  ::dlp::RequestFileAccessResponse access_response;
  access_response.set_allowed(true);
  EXPECT_CALL(request_file_access_call,
              Run(testing::Property(
                      &::dlp::RequestFileAccessRequest::destination_component,
                      ::dlp::DlpComponent::GOOGLE_DRIVE),
                  base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));

  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
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
  EXPECT_CALL(request_file_access_call,
              Run(testing::Property(
                      &::dlp::RequestFileAccessRequest::destination_component,
                      ::dlp::DlpComponent::GOOGLE_DRIVE),
                  base::test::IsNotNullCallback()))
      .WillOnce(
          base::test::RunOnceCallback<1>(access_response, base::ScopedFD()));

  EXPECT_CALL(*files_controller_, MapFilePathToPolicyComponent)
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
      .WillOnce(testing::Return(absl::nullopt))   // destination component
      .WillOnce(testing::Return(absl::nullopt));  // source component

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
  files_controller_->RequestCopyAccess(source, destination,
                                       future.GetCallback());
  EXPECT_FALSE(future.Get()->is_allowed());
}

}  // namespace policy
