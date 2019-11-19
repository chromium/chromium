// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/test/webview_instrumentation_test_native_jni/MemoryMetricsLoggerUtils_jni.h"

#include "android_webview/browser/metrics/memory_metrics_logger.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"

namespace android_webview {

// static
jboolean JNI_MemoryMetricsLoggerUtils_ForceRecordHistograms(JNIEnv* env) {
  auto* memory_metrics_logger = MemoryMetricsLogger::GetInstanceForTesting();
  if (!memory_metrics_logger)
    return false;

  TestTimeouts::Initialize();
  base::test::SingleThreadTaskEnvironment task_environment;
  base::RunLoop run_loop;
  bool result = false;
  memory_metrics_logger->ScheduleRecordForTesting(
      base::BindLambdaForTesting([&](bool success) {
        result = success;
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

}  // namespace android_webview
