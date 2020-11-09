// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_file_system.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_smb_provider_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

namespace chromeos {
namespace smb_client {
namespace {

const file_system_provider::ProviderId kProviderId =
    file_system_provider::ProviderId::CreateFromNativeId("smb");
constexpr char kSharePath[] = "\\\\server\\foobar";
constexpr int32_t kMountId = 4;
constexpr char kDirectoryPath[] = "foo/bar";

class MockSmbProviderClient : public chromeos::FakeSmbProviderClient {
 public:
  MockSmbProviderClient()
      : FakeSmbProviderClient(true /* should_run_synchronously */) {}

  MOCK_METHOD(void,
              DeleteEntry,
              (int32_t, const base::FilePath&, bool, StatusCallback),
              (override));
};

class SmbFileSystemTest : public testing::Test {
 protected:
  SmbFileSystemTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  void SetUp() override {
    ProvidedFileSystemInfo file_system_info(
        kProviderId, {}, base::FilePath(kSharePath), false /* configurable */,
        false /* watchable */, extensions::SOURCE_NETWORK,
        chromeos::file_system_provider::IconSet());
    file_system_ = std::make_unique<SmbFileSystem>(
        file_system_info,
        base::BindRepeating(
            [](const ProvidedFileSystemInfo&) { return kMountId; }),
        SmbFileSystem::UnmountCallback(),
        SmbFileSystem::RequestCredentialsCallback(),
        SmbFileSystem::RequestUpdatedSharePathCallback());

    std::unique_ptr<MockSmbProviderClient> mock_client =
        std::make_unique<MockSmbProviderClient>();
    mock_client_ = mock_client.get();
    // The mock needs to be marked as leaked because ownership is passed to
    // DBusThreadManager.
    testing::Mock::AllowLeak(mock_client.get());
    chromeos::DBusThreadManager::GetSetterForTesting()->SetSmbProviderClient(
        std::move(mock_client));
  }

  void TearDown() override {
    // Because the mock is potentially leaked, expectations needs to be manually
    // verified.
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(mock_client_));
  }

  content::BrowserTaskEnvironment task_environment_;
  MockSmbProviderClient* mock_client_;  // Owned by DBusThreadManager.
  std::unique_ptr<SmbFileSystem> file_system_;
};

TEST_F(SmbFileSystemTest, DeleteEntry_NonRecursive) {
  base::FilePath dir(kDirectoryPath);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_client_, DeleteEntry(kMountId, dir, false, _))
      .WillOnce(WithArg<3>(Invoke([](SmbProviderClient::StatusCallback cb) {
        std::move(cb).Run(smbprovider::ErrorType::ERROR_OK);
      })));
  file_system_->DeleteEntry(
      dir, false /* recursive */,
      base::BindLambdaForTesting([&run_loop](base::File::Error error) {
        EXPECT_EQ(error, base::File::FILE_OK);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(SmbFileSystemTest, DeleteEntry_NonRecursiveFailed) {
  base::FilePath dir(kDirectoryPath);

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_client_, DeleteEntry(kMountId, dir, false, _))
      .WillOnce(WithArg<3>(Invoke([](SmbProviderClient::StatusCallback cb) {
        std::move(cb).Run(smbprovider::ErrorType::ERROR_NOT_EMPTY);
      })));
  file_system_->DeleteEntry(
      dir, false /* recursive */,
      base::BindLambdaForTesting([&run_loop](base::File::Error error) {
        EXPECT_EQ(error, base::File::FILE_ERROR_NOT_EMPTY);
        run_loop.Quit();
      }));
  run_loop.Run();
}

}  // namespace
}  // namespace smb_client
}  // namespace chromeos
