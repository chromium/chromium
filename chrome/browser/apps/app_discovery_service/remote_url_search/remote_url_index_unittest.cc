// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_index.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "chrome/browser/apps/app_discovery_service/remote_url_search/remote_url_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class RemoteUrlIndexTest : public testing::Test {
 protected:
  RemoteUrlIndexTest() = default;
  ~RemoteUrlIndexTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() {
    return temp_dir_.GetPath().AppendASCII("storage.json");
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
};

// TODO(crbug.com/1244221): This test just exercises the unimplemented GetApps
// method as a stand-in for proper tests once the logic is implemented.
TEST_F(RemoteUrlIndexTest, GetApps) {
  auto client = std::make_unique<RemoteUrlClient>(GURL("test.url"));
  RemoteUrlIndex index(std::move(client), GetPath());
  EXPECT_EQ(index.GetApps(""), nullptr);
}

// Tests that one iteration of the update loop doesn't crash.
TEST_F(RemoteUrlIndexTest, WaitForUpdate) {
  auto client = std::make_unique<RemoteUrlClient>(GURL("test.url"));
  RemoteUrlIndex index(std::move(client), GetPath());
  task_environment_.AdvanceClock(base::Days(2));
  Wait();
  SUCCEED();
}

}  // namespace apps
