// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_MEMORY_METRICS_LOGGER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_MEMORY_METRICS_LOGGER_H_

#include <jni.h>
#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"

namespace android_webview {

// MemoryMetricsLogger is responsible for logging the memory related heartbeat
// metrics. MemoryMetricsLogger logs metrics at certain intervals for as long
// as it exists.
//
// MemoryMetricsLogger does the logging on a background task runner and stops
// logging once MemoryMetricsLogger is destroyed.
class MemoryMetricsLogger {
 public:
  using RecordCallback = base::OnceCallback<void(bool)>;
  MemoryMetricsLogger();
  ~MemoryMetricsLogger();

 private:
  struct State;

  friend jboolean JNI_MemoryMetricsLoggerUtils_ForceRecordHistograms(
      JNIEnv* env);

  // Returns the single instance, if one was created.
  static MemoryMetricsLogger* GetInstanceForTesting();

  // Schedules recording metrics. Runs |done_callback| when done with the
  // result of recording metrics. |done_callback| is run on a background
  // TaskRunner.
  void ScheduleRecordForTesting(RecordCallback done_callback);

  // Called on the task runner to record metrics after a delay.
  static void RecordMemoryMetricsAfterDelay(scoped_refptr<State> state);

  // Requests the memory related metrics and calls
  // RecordMemoryMetricsAfterDelay() to schedule another logging call. Runs
  // |done_callback| when done.
  static void RecordMemoryMetrics(scoped_refptr<State> state,
                                  RecordCallback done_callback);

  scoped_refptr<State> state_;

  DISALLOW_COPY_AND_ASSIGN(MemoryMetricsLogger);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_MEMORY_METRICS_LOGGER_H_
