// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher_android.h"

#include "base/android/application_status_listener.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
void OnThreadWatcherTask(base::WaitableEvent* event) {
  event->Signal();
}

void PostAndWaitForWatchdogThread(base::WaitableEvent* event) {
  WatchDogThread::PostDelayedTask(
      FROM_HERE,
      base::Bind(&OnThreadWatcherTask, event),
      base::TimeDelta::FromSeconds(0));

  EXPECT_TRUE(event->TimedWait(base::TimeDelta::FromSeconds(1)));
}

void NotifyApplicationStateChange(base::android::ApplicationState state) {
  base::WaitableEvent watchdog_thread_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(state);
  base::RunLoop().RunUntilIdle();

  PostAndWaitForWatchdogThread(&watchdog_thread_event);
}

}  // namespace

TEST(ThreadWatcherAndroidTest, ApplicationStatusNotification) {
  // Do not delay the ThreadWatcherList initialization for this test.
  ThreadWatcherList::g_initialize_delay_seconds = 0;

  content::BrowserTaskEnvironment task_environment;

  std::unique_ptr<WatchDogThread> watchdog_thread_(new WatchDogThread());
  watchdog_thread_->StartAndWaitForTesting();

  EXPECT_FALSE(ThreadWatcherList::g_thread_watcher_list_);


  // Register, and notify the application has just started,
  // and ensure the thread watcher list is created.
  ThreadWatcherAndroid::RegisterApplicationStatusListener();
  ThreadWatcherList::StartWatchingAll(*base::CommandLine::ForCurrentProcess());
  NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  EXPECT_TRUE(ThreadWatcherList::g_thread_watcher_list_);

  // Notify the application has been stopped, and ensure the thread watcher list
  // has been destroyed.
  NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  EXPECT_FALSE(ThreadWatcherList::g_thread_watcher_list_);

  // And again the last transition, STOPPED -> STARTED.
  NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  EXPECT_TRUE(ThreadWatcherList::g_thread_watcher_list_);

  // ThreadWatcherList::StartWatchingAll() creates g_thread_watcher_observer_.
  // This should be released by ThreadWatcherList::StopWatchingAll() in the end
  // of test to not affect other test cases.
  ThreadWatcherList::StopWatchingAll();
}
