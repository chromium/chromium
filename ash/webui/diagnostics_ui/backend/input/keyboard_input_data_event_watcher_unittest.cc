// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input/keyboard_input_data_event_watcher.h"

#include <linux/input-event-codes.h>
#include <linux/input.h>

#include <cstdint>
#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace ash::diagnostics {

namespace {

constexpr uint32_t kFakeEvdevId = 999;
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

// FakeKeyboardInputDataEventWatcher to validate event processing.
class FakeKeyboardInputDataEventWatcher : public KeyboardInputDataEventWatcher {
 public:
  FakeKeyboardInputDataEventWatcher(
      uint32_t evdev_id,
      base::WeakPtr<KeyboardInputDataEventWatcher::Dispatcher> dispatcher)
      : KeyboardInputDataEventWatcher(evdev_id, dispatcher) {}

  ~FakeKeyboardInputDataEventWatcher() override = default;

  // Don't call the actual read functions.
  void DoStart() override {}
  void DoStop() override {}
  void DoOnFileCanReadWithoutBlocking(int fd) override {}
};

class KeyboardInputDataEventWatcherTest : public testing::Test {
 public:
  KeyboardInputDataEventWatcherTest() = default;
  KeyboardInputDataEventWatcherTest(const KeyboardInputDataEventWatcherTest&) =
      delete;
  KeyboardInputDataEventWatcherTest& operator=(
      const KeyboardInputDataEventWatcherTest&) = delete;
  ~KeyboardInputDataEventWatcherTest() override = default;

  void SetUp() override { SetupWatcher(); }

  void TearDown() override { TearDownWatcher(); }

  StubDispatcher* dispatcher() { return &dispatcher_; }

  KeyboardInputDataEventWatcher* watcher() { return watcher_.get(); }

 protected:
  void SetupWatcher() {
    watcher_ = std::make_unique<FakeKeyboardInputDataEventWatcher>(
        kFakeEvdevId, dispatcher()->GetWeakPtr());
  }

  void TearDownWatcher() { watcher_.reset(); }

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
  StubDispatcher dispatcher_;
  std::unique_ptr<FakeKeyboardInputDataEventWatcher> watcher_;
};

// Verifies that regular keydown and keyup are reported.
TEST_F(KeyboardInputDataEventWatcherTest, StandardKeyPressDispatchesKeyEvent) {
  EXPECT_EQ(0ul, dispatcher()->CallCount());
  for (const auto& input : kFakeKeyAPressAndRelease) {
    watcher()->ProcessEvent(input);
  }

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

  for (const auto& input : kUnhandledKeyboardEvdevEvents) {
    watcher()->ProcessEvent(input);
  }

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
