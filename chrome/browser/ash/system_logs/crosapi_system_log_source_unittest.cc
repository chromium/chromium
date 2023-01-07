// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/crosapi_system_log_source.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/crosapi/fake_browser_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace system_logs {

class CrosapiSystemLogSourceTest : public ::testing::Test {
 public:
  CrosapiSystemLogSourceTest()
      : browser_manager_(std::make_unique<crosapi::FakeBrowserManager>()) {}
  CrosapiSystemLogSourceTest(const CrosapiSystemLogSourceTest&) = delete;
  CrosapiSystemLogSourceTest& operator=(const CrosapiSystemLogSourceTest&) =
      delete;
  ~CrosapiSystemLogSourceTest() override = default;

  void SetUp() override {
    // Set Lacros in running state by default.
    browser_manager_->set_is_running(true);
  }

 protected:
  SysLogsSourceCallback fetch_callback() {
    return base::BindOnce(&CrosapiSystemLogSourceTest::OnFetchComplete,
                          base::Unretained(this));
  }

  void SetupBrowserManagerResponse(base::Value response) {
    browser_manager_->SetGetFeedbackDataResponse(std::move(response));
  }

  void SignalMojoDisconnected() { browser_manager_->SignalMojoDisconnected(); }

  void SetLacrosNotRunning() { browser_manager_->set_is_running(false); }

  void SetWaitForMojoDisconnect() {
    browser_manager_->set_wait_for_mojo_disconnect(true);
  }

  void OnFetchComplete(std::unique_ptr<SystemLogsResponse> response) {
    ++num_callback_calls_;
    response_ = *response;
  }

  int num_callback_calls() const { return num_callback_calls_; }
  const SystemLogsResponse& response() const { return response_; }

 private:
  // Creates the necessary browser threads.
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<crosapi::FakeBrowserManager> browser_manager_;

  // Used to verify that OnGetFeedbackData was called the correct number of
  // times.
  int num_callback_calls_ = 0;

  // Stores results from the log source passed into fetch_callback().
  SystemLogsResponse response_;
};

TEST_F(CrosapiSystemLogSourceTest, OnFeedbackData) {
  // Set up FakeBrowserManager to send log data with 1 log entry for
  // the log source.
  base::Value system_log_entries(base::Value::Type::DICTIONARY);
  system_log_entries.SetStringKey("testing log key", "testing log content");
  SetupBrowserManagerResponse(std::move(system_log_entries));

  // Fetch log data and wait until fetch_callback() is called.
  CrosapiSystemLogSource source;
  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  // Verify CrosapiSystemLogSource::OnGetFeedbackData processed log data
  // correctly. The Lacros log entry in the response() should have "Lacros "
  // prefix in log entry key.
  EXPECT_EQ("Lacros testing log key", response().begin()->first);
  EXPECT_EQ("testing log content", response().begin()->second);

  // Simulate the mojo disconnection event received after fetch_callback() runs.
  SignalMojoDisconnected();

  // Verify fetch_callback() has not been called again.
  EXPECT_EQ(1, num_callback_calls());
}

TEST_F(CrosapiSystemLogSourceTest, FetchWhenLacrosNotRunning) {
  // Set up FakeBrowserManager with Lacros not running.
  SetLacrosNotRunning();

  // Fetch log data and wait until fetch_callback() is called.
  CrosapiSystemLogSource source;
  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  // Verify fetch_callback() has been called with empty response.
  EXPECT_EQ(1, num_callback_calls());
  ASSERT_EQ(0U, response().size());

  SignalMojoDisconnected();

  // Verify fetch_callback() has not been called again.
  EXPECT_EQ(1, num_callback_calls());
}

TEST_F(CrosapiSystemLogSourceTest, OnMojoDisconnectedBeforeLogFetched) {
  // Set up FakeBrowserManager to wait for crosapi mojo disconnected event
  // before sending log data back, so that we can simulate the case
  // that crosapi connection will be disconnected before the log data
  // is fetched, i.e. before OnGetFeedbackData is called.
  SetWaitForMojoDisconnect();

  // Fetch log data.
  CrosapiSystemLogSource source;
  source.Fetch(fetch_callback());
  base::RunLoop().RunUntilIdle();

  // Verify fetch_callback() is not called.
  EXPECT_EQ(0, num_callback_calls());
  ASSERT_EQ(0U, response().size());

  // Simulate the mojo disconnection event received by FakeBrowserManager.
  SignalMojoDisconnected();

  // Verify fetch_callback() is called with empty response.
  EXPECT_EQ(1, num_callback_calls());
  ASSERT_EQ(0U, response().size());
}

}  // namespace system_logs
