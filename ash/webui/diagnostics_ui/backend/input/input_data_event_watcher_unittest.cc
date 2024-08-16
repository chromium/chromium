// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/webui/diagnostics_ui/backend/input/input_data_event_watcher.h"

#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>

#include <cstdint>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {

namespace {

constexpr uint32_t kFakeEvdevId = 999;
constexpr char kFakeEvdevPath[] = "/dev/input/event999";
constexpr timeval kFakeTimeval = {.tv_sec = 1493076832, .tv_usec = 526871};

// Sample of events that an InputDataEventWatcher might process.
constexpr struct input_event kFakeKeyCaplPressAndRelease[] = {
    // Begin press on "left alt"
    {kFakeTimeval, EV_MSC, MSC_SCAN, 0x0},
    {kFakeTimeval, EV_KEY, KEY_LEFTALT, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT, 1},
    // Begin press on "left meta"
    {kFakeTimeval, EV_MSC, MSC_SCAN, 0x38},
    {kFakeTimeval, EV_KEY, KEY_LEFTMETA, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT, 1},
    // Release press on "left meta"
    {kFakeTimeval, EV_MSC, MSC_SCAN, 0xdb},
    {kFakeTimeval, EV_KEY, KEY_LEFTMETA, 0},
    {kFakeTimeval, EV_SYN, SYN_REPORT, 1},
    // Release press on "left alt"
    {kFakeTimeval, EV_MSC, MSC_SCAN, 0x38},
    {kFakeTimeval, EV_KEY, KEY_LEFTALT, 0},
    {kFakeTimeval, EV_SYN, SYN_REPORT, 1},
    // Enable LED
    {kFakeTimeval, EV_LED, LED_CAPSL, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT, 1}};

class FakeWatcher : public InputDataEventWatcher {
 public:
  FakeWatcher(uint32_t evdev_id, const base::FilePath& path, int fd)
      : InputDataEventWatcher(evdev_id, path, fd) {}
  ~FakeWatcher() override = default;

  void SetQuitClosure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

  void ProcessEvent(const input_event& event) override {
    events.emplace_back(event);
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
  }

  // Test helpers:
  size_t num_events_processed() { return events.size(); }
  const input_event GetCall(int index) { return events[index]; }

 private:
  std::vector<input_event> events;
  base::OnceClosure quit_closure_;
};

class InputDataEventWatcherTest : public testing::Test {
 public:
  InputDataEventWatcherTest()
      : task_environment_(
            std::make_unique<base::test::SingleThreadTaskEnvironment>(
                base::test::SingleThreadTaskEnvironment::MainThreadType::UI)),
        test_path_(base::FilePath(kFakeEvdevPath)) {}
  InputDataEventWatcherTest(const InputDataEventWatcherTest&) = delete;
  InputDataEventWatcherTest& operator=(const InputDataEventWatcherTest&) =
      delete;
  ~InputDataEventWatcherTest() override = default;

  void SetUp() override { SetupWatcher(); }

  void TearDown() override { TearDownWatcher(); }

  // Write event to write endpoint of pipe created during setup.
  void WriteInputEvent(const input_event& input) {
    int write_result = write(writefd(), &input, sizeof(input));
    if (write_result < 0)
      PLOG(WARNING) << "write";
    ASSERT_TRUE(write_result >= 0);
  }

  FakeWatcher* watcher() { return watcher_.get(); }
  int readfd() { return pipefds_[0]; }
  int writefd() { return pipefds_[1]; }

  // Check expected event data matches event "processed" by FakeWatcher.
  void VerifyInputEvent(const input_event& expected, int call_index) {
    EXPECT_EQ(call_index + 1ul, watcher()->num_events_processed());
    auto written_event = watcher()->GetCall(call_index);
    EXPECT_EQ(expected.type, written_event.type);
    EXPECT_EQ(expected.code, written_event.code);
    EXPECT_EQ(expected.value, written_event.value);
  }

  base::test::SingleThreadTaskEnvironment* task_environment() {
    return task_environment_.get();
  }

 protected:
  void SetupWatcher() {
    // Create pipe to fake having a `/dev/input/event{evdev_id}` file.
    EXPECT_TRUE(base::CreateLocalNonBlockingPipe(pipefds_));
    watcher_ =
        std::make_unique<FakeWatcher>(kFakeEvdevId, test_path_, readfd());
  }

  void TearDownWatcher() {
    watcher()->Stop();
    watcher_.reset();

    // Only close the write endpoint. InputDataEventWatcher::Stop will close
    // down the read endpoint. Calling close on the read will cause BADF.
    if (IGNORE_EINTR(close(writefd())) < 0)
      PLOG(WARNING) << "close (writefd)";
  }

 private:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  int pipefds_[2];
  const base::FilePath test_path_;
  std::unique_ptr<FakeWatcher> watcher_;
};

// Verify that valid input_event can be read and
// trigger`InputDataEventWatcher::ProcessEvent`.
TEST_F(InputDataEventWatcherTest, ReadsInputEventsFromFd) {
  watcher()->Start();
  size_t write_count = 0ul;
  for (const auto& event : kFakeKeyCaplPressAndRelease) {
    base::RunLoop run_loop;
    // Wait for read to trigger `InputDataEventWatcher::ProcessEvent`.
    watcher()->SetQuitClosure(
        base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
    WriteInputEvent(event);
    run_loop.Run();
    // Ensure expected event is passed to ProcessEvent.
    VerifyInputEvent(event, write_count);
    write_count++;
  }
}

}  // namespace

}  // namespace ash::diagnostics
