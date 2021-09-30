// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file integrates ThreadWatcher with Android's activity life-cycle.
// When Activity.onStop() is called, in order to preserve battery, it will
// deactive the thread watcher. Conversely, when onRestart() is called,
// it will reactivate.
// See more details in:
// http://developer.android.com/training/basics/activity-lifecycle/stopping.html

#ifndef CHROME_BROWSER_METRICS_THREAD_WATCHER_ANDROID_H_
#define CHROME_BROWSER_METRICS_THREAD_WATCHER_ANDROID_H_

#include "base/macros.h"

class ThreadWatcherAndroid {
 public:
  ThreadWatcherAndroid() = delete;
  ThreadWatcherAndroid(const ThreadWatcherAndroid&) = delete;
  ThreadWatcherAndroid& operator=(const ThreadWatcherAndroid&) = delete;

  static void RegisterApplicationStatusListener();
};

#endif  // CHROME_BROWSER_METRICS_THREAD_WATCHER_ANDROID_H_
