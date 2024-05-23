// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/download_controller_ash.h"

#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/crosapi/mojom/download_controller.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crosapi {

// Helpers ---------------------------------------------------------------------

mojom::DownloadItemPtr CreateDownloadItemWithStartTimeOffset(
    base::TimeDelta start_time_offset) {
  auto download = mojom::DownloadItem::New();
  download->start_time = base::Time::Now() + start_time_offset;
  return download;
}

bool IsSortedChronologicallyByStartTime(
    const std::vector<mojom::DownloadItemPtr>& downloads) {
  for (size_t i = 1; i < downloads.size(); ++i) {
    if (downloads[i]->start_time.value_or(base::Time()) <
        downloads[i - 1]->start_time.value_or(base::Time())) {
      return false;
    }
  }
  return true;
}

// Mocks -----------------------------------------------------------------------

class MockDownloadControllerClient : public mojom::DownloadControllerClient {
 public:
  MOCK_METHOD(void,
              GetAllDownloads,
              (mojom::DownloadControllerClient::GetAllDownloadsCallback),
              (override));
  MOCK_METHOD(void, Pause, (const std::string& download_guid), (override));
  MOCK_METHOD(void,
              Resume,
              (const std::string& download_guid, bool user_resume),
              (override));
  MOCK_METHOD(void,
              Cancel,
              (const std::string& download_guid, bool user_cancel),
              (override));
  MOCK_METHOD(void,
              SetOpenWhenComplete,
              (const std::string& download_guid, bool open_when_complete),
              (override));
};

// DownloadControllerAshTest ---------------------------------------------------

class DownloadControllerAshTest : public testing::Test {
 public:
  DownloadControllerAsh* download_controller_ash() {
    return &download_controller_ash_;
  }

  std::vector<mojom::DownloadItemPtr> GetAllDownloads() {
    base::test::TestFuture<std::vector<mojom::DownloadItemPtr>> future;
    download_controller_ash()->GetAllDownloads(future.GetCallback());
    return future.Take();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  DownloadControllerAsh download_controller_ash_;
};

// Tests -----------------------------------------------------------------------

TEST_F(DownloadControllerAshTest, GetAllDownloads_NoBoundClients) {
  // Invoke `GetAllDownloads()` with no clients bound.
  const std::vector<mojom::DownloadItemPtr> downloads = GetAllDownloads();

  EXPECT_EQ(downloads.size(), 0u);
}

TEST_F(DownloadControllerAshTest, GetAllDownloads_MultipleBoundClients) {
  // Bind `client1`.
  testing::NiceMock<MockDownloadControllerClient> client1;
  mojo::Receiver<mojom::DownloadControllerClient> client1_receiver{&client1};
  download_controller_ash()->BindClient(
      client1_receiver.BindNewPipeAndPassRemoteWithVersion());

  // Mock `client1` response for `GetAllDownloads()`.
  EXPECT_CALL(client1, GetAllDownloads)
      .WillOnce(testing::Invoke(
          [](mojom::DownloadControllerClient::GetAllDownloadsCallback
                 callback) {
            std::vector<mojom::DownloadItemPtr> downloads;
            downloads.push_back(
                CreateDownloadItemWithStartTimeOffset(base::Minutes(10)));
            downloads.push_back(
                CreateDownloadItemWithStartTimeOffset(-base::Minutes(10)));
            std::move(callback).Run(std::move(downloads));
          }));

  // Bind `client2`.
  testing::NiceMock<MockDownloadControllerClient> client2;
  mojo::Receiver<mojom::DownloadControllerClient> client2_receiver{&client2};
  download_controller_ash()->BindClient(
      client2_receiver.BindNewPipeAndPassRemoteWithVersion());

  // Mock `client2` response for `GetAllDownloads()`.
  EXPECT_CALL(client2, GetAllDownloads)
      .WillOnce(testing::Invoke(
          [](mojom::DownloadControllerClient::GetAllDownloadsCallback
                 callback) {
            std::vector<mojom::DownloadItemPtr> downloads;
            downloads.push_back(
                CreateDownloadItemWithStartTimeOffset(base::Minutes(20)));
            downloads.push_back(
                CreateDownloadItemWithStartTimeOffset(-base::Minutes(20)));
            std::move(callback).Run(std::move(downloads));
          }));

  // Invoke `GetAllDownloads()`.
  const std::vector<mojom::DownloadItemPtr> downloads = GetAllDownloads();

  EXPECT_EQ(downloads.size(), 4u);
  EXPECT_TRUE(IsSortedChronologicallyByStartTime(downloads));
}

}  // namespace crosapi
