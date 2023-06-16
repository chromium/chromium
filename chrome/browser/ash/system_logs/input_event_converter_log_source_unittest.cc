// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "chrome/browser/ash/system_logs/input_event_converter_log_source.h"

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchpad_device.h"

namespace system_logs {

constexpr char kResponseKey[] = "ozone_evdev_input_event_converters";

class InputEventConverterLogSourceTest : public ::testing::Test {
 public:
  InputEventConverterLogSourceTest() {}
  InputEventConverterLogSourceTest(const InputEventConverterLogSourceTest&) =
      delete;
  InputEventConverterLogSourceTest& operator=(
      const InputEventConverterLogSourceTest&) = delete;
  ~InputEventConverterLogSourceTest() override = default;

 protected:
  size_t num_callback_calls() const { return num_callback_calls_; }
  const SystemLogsResponse& response() const { return response_; }

  // Synchronously invokes Fetch on a log source.
  void RunSourceFetch(InputEventConverterLogSource* source) {
    source->Fetch(
        base::BindOnce(&InputEventConverterLogSourceTest::OnFetchComplete,
                       base::Unretained(this)));

    // Start the loop, the source fetch will exit.
    fetch_run_loop_.Run();
  }

 private:
  void OnFetchComplete(std::unique_ptr<SystemLogsResponse> response) {
    ++num_callback_calls_;
    response_ = *response;
    fetch_run_loop_.Quit();
  }

  // Creates the necessary browser threads.
  content::BrowserTaskEnvironment task_environment_;

  // Used to verify that OnGetFeedbackData was called the correct number of
  // times.
  size_t num_callback_calls_ = 0;

  // Stores results from the log source passed into fetch_callback().
  SystemLogsResponse response_;
  // Used for waiting on fetch results.
  base::RunLoop fetch_run_loop_;
};

TEST_F(InputEventConverterLogSourceTest, Nil) {
  InputEventConverterLogSource source;

  RunSourceFetch(&source);

  // Verify fetch_callback() has been called.
  EXPECT_EQ(1U, num_callback_calls());
  ASSERT_EQ(1U, response().size());

  auto it = response().find(kResponseKey);
  ASSERT_NE(it, response().end());

  // No input devices present, so response should be empty.
  EXPECT_EQ("", it->second);
}

// TODO(b/284543874): As there is currently no way to mock the InputController
// within the test browser environment, we cannot add input devices visible
// to InputEventConverterLogSource. The next significant layer is tested by
// InputDeviceFactoryEvdevTest::DescribeForLog*.

}  // namespace system_logs
