// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/single_debug_daemon_log_source.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

using SupportedSource = SingleDebugDaemonLogSource::SupportedSource;

class SingleDebugDaemonLogSourceTest : public ::testing::Test {
 public:
  SingleDebugDaemonLogSourceTest() : num_callback_calls_(0) {}

  SingleDebugDaemonLogSourceTest(const SingleDebugDaemonLogSourceTest&) =
      delete;
  SingleDebugDaemonLogSourceTest& operator=(
      const SingleDebugDaemonLogSourceTest&) = delete;

  void SetUp() override {
    // Since no debug daemon will be available during a unit test, use
    // FakeDebugDaemonClient to provide dummy DebugDaemonClient functionality.
    ash::DebugDaemonClient::InitializeFake();
  }

  void TearDown() override { ash::DebugDaemonClient::Shutdown(); }

 protected:
  SysLogsSourceCallback fetch_callback() {
    return base::BindOnce(&SingleDebugDaemonLogSourceTest::OnFetchComplete,
                          base::Unretained(this));
  }

  int num_callback_calls() const { return num_callback_calls_; }

  const SystemLogsResponse& response() const { return response_; }

  void ClearResponse() { response_.clear(); }

 private:
  void OnFetchComplete(std::unique_ptr<SystemLogsResponse> response) {
    ++num_callback_calls_;
    response_ = *response;
  }

  // Creates the necessary browser threads.
  content::BrowserTaskEnvironment task_environment_;

  // Used to verify that OnFetchComplete was called the correct number of times.
  int num_callback_calls_;

  // Stores results from the log source.
  SystemLogsResponse response_;
};

TEST_F(SingleDebugDaemonLogSourceTest, SingleCall) {
  SingleDebugDaemonLogSource source(SupportedSource::kModetest);

  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  EXPECT_EQ("modetest", response().begin()->first);
  EXPECT_EQ("modetest: response from GetLog", response().begin()->second);
}

TEST_F(SingleDebugDaemonLogSourceTest, MultipleCalls) {
  SingleDebugDaemonLogSource source(SupportedSource::kLsusb);

  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  EXPECT_EQ("lsusb", response().begin()->first);
  EXPECT_EQ("lsusb: response from GetLog", response().begin()->second);

  ClearResponse();

  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  EXPECT_EQ("lsusb", response().begin()->first);
  EXPECT_EQ("lsusb: response from GetLog", response().begin()->second);

  ClearResponse();

  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(3, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  EXPECT_EQ("lsusb", response().begin()->first);
  EXPECT_EQ("lsusb: response from GetLog", response().begin()->second);
}

TEST_F(SingleDebugDaemonLogSourceTest, MultipleSources) {
  SingleDebugDaemonLogSource source1(SupportedSource::kModetest);
  source1.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  EXPECT_EQ("modetest", response().begin()->first);
  EXPECT_EQ("modetest: response from GetLog", response().begin()->second);

  ClearResponse();

  SingleDebugDaemonLogSource source2(SupportedSource::kLsusb);
  source2.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  EXPECT_EQ("lsusb", response().begin()->first);
  EXPECT_EQ("lsusb: response from GetLog", response().begin()->second);
}

}  // namespace system_logs
