// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/keyboard_input_data_event_watcher.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdint>
#include <memory>

#include "ash/webui/diagnostics_ui/backend/input_data_event_watcher.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {

namespace {

constexpr char kFakeDevicePath[] = "/dev/input/test-device";
constexpr uint32_t kFakeEvdevId{999};
constexpr uint32_t kScanCodeKeyA = 0x1E;
constexpr bool kKeyPressed = 1;
constexpr bool kKeyReleased = 0;
// Timeval is arbitrary as value is not used except as part of the
constexpr timeval kFakeTimeval = {.tv_sec = 1493076832, .tv_usec = 526871};

// Events fired when a alphanumeric key is pressed.
constexpr struct input_event kFakeKeyAPressAndRelease[] = {
    // input events for "A" key down.
    {kFakeTimeval, EV_MSC, MSC_SCAN, kScanCodeKeyA},
    {kFakeTimeval, EV_KEY, KEY_A, kKeyPressed},
    {kFakeTimeval, EV_SYN, SYN_REPORT},
    // input events for "A" key up.
    {kFakeTimeval, EV_MSC, 4, kScanCodeKeyA},
    {kFakeTimeval, EV_KEY, KEY_A, kKeyReleased},
    {kFakeTimeval, EV_SYN, SYN_REPORT}

};

// Unhandled keyboard Evdev codes. Note the value is set to "1" arbitrarily.
// Set of unhandled events does not cover all possible events only the events
// available and unhandled from the internal keyboard of a Magolor device.
constexpr struct input_event kUnhandledKeyboardEvdevEvents[] = {
    {kFakeTimeval, EV_SYN, SYN_REPORT},
    // input events for "caplocks" LED on.
    {kFakeTimeval, EV_LED, LED_CAPSL, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT},

    // input events for "numlock" LED on.
    {kFakeTimeval, EV_LED, LED_NUML, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT},

    // input events for "scroll" LED on.
    {kFakeTimeval, EV_LED, LED_SCROLLL, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT},

    // input events for auto repeat delay.
    {kFakeTimeval, EV_REP, REP_DELAY, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT},

    // input events for auto repeat period.
    {kFakeTimeval, EV_REP, REP_PERIOD, 1},
    {kFakeTimeval, EV_SYN, SYN_REPORT}};

struct KeyEventParams {
  uint32_t id;
  uint32_t key_code;
  uint32_t scan_code;
  bool down;
  KeyEventParams(uint32_t id, uint32_t key_code, uint32_t scan_code, bool down)
      : id(id), key_code(key_code), scan_code(scan_code), down(down) {}
  ~KeyEventParams() = default;
};

// Simple stub that tracks dispatch events.
class StubDispatcher : public KeyboardInputDataEventWatcher::Dispatcher {
 public:
  StubDispatcher() = default;
  ~StubDispatcher() override = default;

  void SendInputKeyEvent(uint32_t id,
                         uint32_t key_code,
                         uint32_t scan_code,
                         bool down) override {
    calls_.emplace_back(id, key_code, scan_code, down);
  }

  size_t CallCount() const { return calls_.size(); }

  KeyEventParams GetCall(size_t index) const {
    DCHECK(index >= 0ul && index < calls_.size());

    return calls_[index];
  }

  base::WeakPtr<StubDispatcher> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::vector<KeyEventParams> calls_;
  base::WeakPtrFactory<StubDispatcher> weak_factory_{this};
};

class KeyboardInputDataEventWatcherTest : public testing::Test {
 public:
  KeyboardInputDataEventWatcherTest()
      : task_environment_(
            std::make_unique<base::test::SingleThreadTaskEnvironment>(
                base::test::SingleThreadTaskEnvironment::MainThreadType::UI)),
        dispatcher_{} {}
  KeyboardInputDataEventWatcherTest(const KeyboardInputDataEventWatcherTest&) =
      delete;
  KeyboardInputDataEventWatcherTest& operator=(
      const KeyboardInputDataEventWatcherTest&) = delete;
  ~KeyboardInputDataEventWatcherTest() override = default;

  void SetUp() override { SetupWatcher(); }

  void TearDown() override { TearDownWatcher(); }

  void WriteInputEvent(const input_event& input) {
    int write_result = write(pipefds_[1], &input, sizeof(input));
    if (write_result < 0)
      PLOG(WARNING) << "write";
    ASSERT_TRUE(write_result >= 0);
  }

  StubDispatcher* dispatcher() { return &dispatcher_; }

  KeyboardInputDataEventWatcher* watcher() { return watcher_.get(); }

 protected:
  void SetupWatcher() {
    int pipe_result = (pipe2(pipefds_, O_NONBLOCK));
    if (pipe_result < 0)
      PLOG(WARNING) << "pipe";
    ASSERT_EQ(0, pipe_result);
    watcher_ = std::make_unique<KeyboardInputDataEventWatcher>(
        kFakeEvdevId, fake_device_path, pipefds_[0],
        dispatcher()->GetWeakPtr());
  }

  void TearDownWatcher() {
    watcher()->Stop();

    // Only close the write endpoint. InputDataEventWatcher::Stop will close
    // down the read endpoint. Calling close on the read will cause BADF.
    if (IGNORE_EINTR(close(pipefds_[1])) < 0)
      PLOG(WARNING) << "close";
    watcher_.reset();
  }

  // Helper for call assertions.
  void VerifyDispatcherCall(size_t index,
                            uint32_t expected_key_code,
                            bool expected_down) {
    auto dispatch_call = dispatcher_.GetCall(index);
    EXPECT_EQ(kFakeEvdevId, dispatch_call.id);
    EXPECT_EQ(expected_key_code, dispatch_call.key_code);
    EXPECT_EQ(expected_down, dispatch_call.down);
  }

 private:
  std::unique_ptr<base::test::SingleThreadTaskEnvironment> task_environment_;
  base::FilePath fake_device_path{kFakeDevicePath};
  StubDispatcher dispatcher_;
  int pipefds_[2];
  std::unique_ptr<KeyboardInputDataEventWatcher> watcher_;
};

// Verifies that regular keydown and keyup are reported.
TEST_F(KeyboardInputDataEventWatcherTest, StandardKeyPressDispatchesKeyEvent) {
  EXPECT_EQ(0ul, dispatcher()->CallCount());

  watcher()->Start();
  base::RunLoop run_loop;
  watcher()->SetQuitClosureForTesting(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));

  for (const auto& input : kFakeKeyAPressAndRelease) {
    WriteInputEvent(input);
  }
  run_loop.Run();

  //  Two events dispatched.
  EXPECT_EQ(2ul, dispatcher()->CallCount());
  // Event 1: "A" key down.
  VerifyDispatcherCall(/*index=*/0, KEY_A,
                       /*expected_down=*/kKeyPressed);
  // Event 2: "A" key up.
  VerifyDispatcherCall(/*index=*/1, KEY_A,
                       /*expected_down=*/kKeyReleased);
}

// Verifies that unknown event codes are reported as `KEY_RESERVED` released.
TEST_F(KeyboardInputDataEventWatcherTest, UnknownEvCodes) {
  EXPECT_EQ(0ul, dispatcher()->CallCount());

  watcher()->Start();
  for (const auto& input : kUnhandledKeyboardEvdevEvents) {
    WriteInputEvent(input);
  }
  base::RunLoop run_loop;
  watcher()->SetQuitClosureForTesting(
      base::BindLambdaForTesting([&]() { run_loop.Quit(); }));
  run_loop.Run();

  // Two events dispatched.
  EXPECT_EQ(6ul, dispatcher()->CallCount());
  for (size_t i = 0; i < dispatcher()->CallCount(); i++) {
    // Default empty event state:
    VerifyDispatcherCall(/*index=*/i, KEY_RESERVED,
                         /*expected_down=*/kKeyReleased);
  }
}

}  // namespace

}  // namespace ash::diagnostics
