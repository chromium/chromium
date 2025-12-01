// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/memory_metrics_logger.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/webview_instrumentation_test_native_jni/MemoryMetricsLoggerUtils_jni.h"

namespace android_webview {

// static
static jboolean JNI_MemoryMetricsLoggerUtils_ForceRecordHistograms(
    JNIEnv* env) {
  // Note: Using a WaitableEvent instead of a RunLoop because there is no
  // sequenced context. Also can't use a TaskEnvironment because this function
  // is not running on the main thread.
  CHECK(!base::SequencedTaskRunner::HasCurrentDefault());
  CHECK(!base::SingleThreadTaskRunner::GetMainThreadDefault()
             ->BelongsToCurrentThread());

  auto* memory_metrics_logger =
      ::metrics::MemoryMetricsLogger::GetInstanceForTesting();
  if (!memory_metrics_logger) {
    return false;
  }

  TestTimeouts::Initialize();
  base::WaitableEvent waitable_event;
  bool result = false;
  memory_metrics_logger->ScheduleRecordForTesting(
      base::BindLambdaForTesting([&](bool success) {
        result = success;
        waitable_event.Signal();
      }));
  waitable_event.TimedWait(TestTimeouts::action_timeout());
  return result;
}

}  // namespace android_webview

DEFINE_JNI(MemoryMetricsLoggerUtils)
