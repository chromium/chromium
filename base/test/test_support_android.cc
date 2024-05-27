// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/looper.h>
#include <stdarg.h>
#include <string.h>

#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_android.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/multiprocess_test.h"

namespace {

base::FilePath* g_test_data_dir = nullptr;
uint32_t g_non_delayed_enter_count;

struct RunState {
  RunState(base::MessagePump::Delegate* delegate, int run_depth)
      : delegate(delegate),
        run_depth(run_depth),
        should_quit(false) {
  }

  raw_ptr<base::MessagePump::Delegate> delegate;

  // Used to count how many Run() invocations are on the stack.
  int run_depth;

  // Used to flag that the current Run() invocation should return ASAP.
  bool should_quit;
};

RunState* g_state = nullptr;

// A singleton WaitableEvent wrapper so we avoid a busy loop in
// MessagePumpAndroidStub. Other platforms use the native event loop which
// blocks when there are no pending messages.
class Waitable {
 public:
  static Waitable* GetInstance() {
    return base::Singleton<Waitable,
                           base::LeakySingletonTraits<Waitable>>::get();
  }

  Waitable(const Waitable&) = delete;
  Waitable& operator=(const Waitable&) = delete;

  // Signals that there are more work to do.
  void Signal() { waitable_event_.Signal(); }

  // Blocks until more work is scheduled.
  void Block() { waitable_event_.Wait(); }

  void Quit() {
    g_state->should_quit = true;
    Signal();
  }

 private:
  friend struct base::DefaultSingletonTraits<Waitable>;

  Waitable()
      : waitable_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  base::WaitableEvent waitable_event_;
};

// The MessagePumpAndroid implementation for test purpose.
class MessagePumpAndroidStub : public base::MessagePumpAndroid {
 public:
  MessagePumpAndroidStub() { Waitable::GetInstance(); }
  ~MessagePumpAndroidStub() override = default;

  // In tests, there isn't a native thread, as such RunLoop::Run() should be
  // used to run the loop instead of attaching and delegating to the native
  // loop. As such, this override ignores the Attach() request.
  void Attach(base::MessagePump::Delegate* delegate) override {}

  void Run(base::MessagePump::Delegate* delegate) override {
    // The following was based on message_pump_glib.cc, except we're using a
    // WaitableEvent since there are no native message loop to use.
    RunState state(delegate, g_state ? g_state->run_depth + 1 : 1);

    RunState* previous_state = g_state;
    g_state = &state;

    // When not nested we can use the looper, otherwise fall back
    // to the stub implementation.
    if (g_state->run_depth > 1) {
      RunNested(delegate);
    } else {
      SetQuit(false);
      SetDelegate(delegate);

      // Pump the loop once in case we're starting off idle as ALooper_pollOnce
      // will never return in that case.
      ScheduleWork();
      while (true) {
        // Waits for either the delayed, or non-delayed fds to be signalled,
        // calling either OnDelayedLooperCallback, or
        // OnNonDelayedLooperCallback, respectively. This uses Android's Looper
        // implementation, which is based off of epoll.
        ALooper_pollOnce(-1, nullptr, nullptr, nullptr);
        if (ShouldQuit())
          break;
      }
    }

    g_state = previous_state;
  }

  void OnNonDelayedLooperCallback() override {
    g_non_delayed_enter_count++;
    base::MessagePumpAndroid::OnNonDelayedLooperCallback();
  }

  void RunNested(base::MessagePump::Delegate* delegate) {
    bool more_work_is_plausible = true;

    for (;;) {
      if (!more_work_is_plausible) {
        Waitable::GetInstance()->Block();
        if (g_state->should_quit)
          break;
      }

      Delegate::NextWorkInfo next_work_info = g_state->delegate->DoWork();
      more_work_is_plausible = next_work_info.is_immediate();
      if (g_state->should_quit)
        break;

      if (more_work_is_plausible)
        continue;

      g_state->delegate->DoIdleWork();
      if (g_state->should_quit)
        break;

      more_work_is_plausible |= !next_work_info.delayed_run_time.is_max();
    }
  }

  void Quit() override {
    CHECK(g_state);
    if (g_state->run_depth > 1) {
      Waitable::GetInstance()->Quit();
    } else {
      MessagePumpAndroid::Quit();
    }
  }

  void ScheduleWork() override {
    if (g_state && g_state->run_depth > 1) {
      Waitable::GetInstance()->Signal();
    } else {
      MessagePumpAndroid::ScheduleWork();
    }
  }

  void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override {
    if (g_state && g_state->run_depth > 1) {
      Waitable::GetInstance()->Signal();
    } else {
      MessagePumpAndroid::ScheduleDelayedWork(next_work_info);
    }
  }
};

std::unique_ptr<base::MessagePump> CreateMessagePumpAndroidStub() {
  auto message_pump_stub = std::make_unique<MessagePumpAndroidStub>();
  message_pump_stub->set_is_type_ui(true);
  return message_pump_stub;
}

// Provides the test path for paths overridden during tests.
bool GetTestProviderPath(int key, base::FilePath* result) {
  switch (key) {
    // On Android, our tests don't have permission to write to DIR_MODULE.
    // gtest/test_runner.py pushes data to external storage.
    // TODO(agrieve): Stop overriding DIR_ANDROID_APP_DATA.
    // https://crbug.com/617734
    // Instead DIR_ASSETS should be used to discover assets file location in
    // tests.
    case base::DIR_ANDROID_APP_DATA:
    case base::DIR_ASSETS:
    case base::DIR_SRC_TEST_DATA_ROOT:
    case base::DIR_OUT_TEST_DATA_ROOT:
      CHECK(g_test_data_dir != nullptr);
      *result = *g_test_data_dir;
      return true;
    default:
      return false;
  }
}

void InitPathProvider(int key) {
  base::FilePath path;
  // If failed to override the key, that means the way has not been registered.
  if (GetTestProviderPath(key, &path) &&
      !base::PathService::Override(key, path)) {
    base::PathService::RegisterProvider(&GetTestProviderPath, key, key + 1);
  }
}

}  // namespace

namespace base {

void InitAndroidTestPaths(const FilePath& test_data_dir) {
  if (g_test_data_dir) {
    CHECK(test_data_dir == *g_test_data_dir);
    return;
  }
  g_test_data_dir = new FilePath(test_data_dir);
  InitPathProvider(DIR_ANDROID_APP_DATA);
  InitPathProvider(DIR_ASSETS);
  InitPathProvider(DIR_SRC_TEST_DATA_ROOT);
  InitPathProvider(DIR_OUT_TEST_DATA_ROOT);
}

void InitAndroidTestMessageLoop() {
  // NOTE something else such as a JNI call may have already overridden the UI
  // factory.
  if (!MessagePump::IsMessagePumpForUIFactoryOveridden())
    MessagePump::OverrideMessagePumpForUIFactory(&CreateMessagePumpAndroidStub);
}

uint32_t GetAndroidNonDelayedWorkEnterCount() {
  return g_non_delayed_enter_count;
}

}  // namespace base
