// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "components/embedder_support/android/metrics/memory_metrics_logger.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/test/webview_instrumentation_test_native_jni/MemoryMetricsLoggerUtils_jni.h"

namespace android_webview {

// static
jboolean JNI_MemoryMetricsLoggerUtils_ForceRecordHistograms(JNIEnv* env) {
  auto* memory_metrics_logger =
      ::metrics::MemoryMetricsLogger::GetInstanceForTesting();
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
