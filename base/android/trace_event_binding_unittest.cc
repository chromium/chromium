// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/trace_event_binding.h"

#include <jni.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/test_trace_processor.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize `FromJniType()` / `ToJniType()`.
#include "base/base_unittest_support_jni/TraceEventNativeTestSupport_jni.h"

namespace base::android {

namespace {

using ::testing::ElementsAre;
namespace webview_pb =
    ::perfetto::protos::pbzero::perfetto_pbzero_enum_WebViewStartup;

class TraceEventBindingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    JNIEnv* env = AttachCurrentThread();
    was_enabled_ = Java_TraceEventNativeTestSupport_enabled(env);
    Java_TraceEventNativeTestSupport_setEnabled(env, true);
  }

  void TearDown() override {
    JNIEnv* env = AttachCurrentThread();
    Java_TraceEventNativeTestSupport_setEnabled(env, was_enabled_);
  }

  static base::expected<base::test::TestTraceProcessor::QueryResult,
                        std::string>
  QueryStartupSlices(base::test::TestTraceProcessor& ttp) {
    return ttp.RunQuery(R"(
      SELECT
        s.name,
        s.ts / 1000000 AS ts_ms,
        s.dur / 1000000 AS dur_ms,
        pt.name AS track_name
      FROM slice s
      JOIN process_track pt ON s.track_id = pt.id
      WHERE s.category = 'android_webview.timeline'
      ORDER BY s.ts ASC, s.dur DESC, s.name ASC
    )");
  }

  static base::expected<base::test::TestTraceProcessor::QueryResult,
                        std::string>
  QueryStartupArgs(base::test::TestTraceProcessor& ttp) {
    return ttp.RunQuery(R"(
      SELECT
        EXTRACT_ARG(arg_set_id,
                    'webview_startup.start_call_site') AS start_site,
        EXTRACT_ARG(arg_set_id,
                    'webview_startup.finish_call_site') AS finish_site,
        EXTRACT_ARG(arg_set_id, 'webview_startup.startup_mode') AS mode
      FROM slice
      WHERE name = 'WebView.Startup.CreationTime.StartChromiumLocked'
    )");
  }

  base::test::TaskEnvironment task_environment_;
  base::test::TracingEnvironment tracing_environment_;
  bool was_enabled_ = false;
};

// Verifies that retroactive, millisecond-resolution WebView startup slices
// keep their exact durations when emitted out of order or sharing millisecond
// boundaries with each other and with live thread slices.
TEST_F(TraceEventBindingTest, WebViewStartupOutOfOrderAndOverlappingSlices) {
  JNIEnv* env = AttachCurrentThread();
  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace("android_webview.timeline,test_ui");
  Java_TraceEventNativeTestSupport_setEnabled(env, true);

  const auto thread_track = perfetto::ThreadTrack::Current();

  // Open a live microsecond-resolution slice on the current thread whose end
  // timestamp (116.0 ms) lands on the exact millisecond boundary shared by
  // `TotalFactoryInitTime`, `Stage1.FactoryInit`, `StartChromiumLocked`, and
  // `FirstInstanceWithGlobalStartup`.
  TRACE_EVENT_BEGIN("test_ui", "LiveThreadSlice", thread_track,
                    TimeTicks() + Microseconds(110400));

  // Mirror `StartupDelegateImpl.onStartupDiagnosticsReady()` emission order:
  // `StartChromiumLocked` is emitted before `TotalFactoryInitTime` and
  // `Stage1.FactoryInit` even though it started at their end millisecond,
  // followed by a wrapping `FirstInstanceWithGlobalStartup` slice on the same
  // start millisecond and a 0-ms `NotFirstInstance` slice on the end boundary.
  Java_TraceEventNativeTestSupport_webViewStartupStartChromiumLocked(
      env, 116, 25, static_cast<int>(webview_pb::WEBVIEW_INSTANCE_LOAD_URL),
      static_cast<int>(webview_pb::ASYNC_WEBVIEW_STARTUP),
      static_cast<int>(webview_pb::PARTIAL_ASYNC_THEN_SYNC));
  Java_TraceEventNativeTestSupport_webViewStartupTotalFactoryInit(env, 100, 16);
  Java_TraceEventNativeTestSupport_webViewStartupStage1(env, 104, 12);
  Java_TraceEventNativeTestSupport_webViewStartupFirstInstance(
      env, 116, 35, /*includedGlobalStartup=*/true);
  Java_TraceEventNativeTestSupport_webViewStartupNotFirstInstance(env, 151, 0);

  TRACE_EVENT_END("test_ui", thread_track, TimeTicks() + Microseconds(116000));

  absl::Status status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto startup_slices = QueryStartupSlices(test_trace_processor);
  ASSERT_TRUE(startup_slices.has_value()) << startup_slices.error();
  EXPECT_THAT(
      startup_slices.value(),
      ElementsAre(
          std::vector<std::string>{"name", "ts_ms", "dur_ms", "track_name"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.TotalFactoryInitTime", "100", "16",
              "WebViewStartup"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.Stage1.FactoryInit", "104", "12",
              "WebViewStartup"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.FirstInstanceWithGlobalStartup",
              "116", "35", "WebViewStartup"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.StartChromiumLocked", "116", "25",
              "WebViewStartup"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.NotFirstInstance", "151", "0",
              "WebViewStartup"}));

  auto thread_slice = test_trace_processor.RunQuery(R"(
    SELECT
      s.name,
      s.dur / 1000 AS dur_us
    FROM slice s
    JOIN thread_track tt ON s.track_id = tt.id
    WHERE s.category = 'test_ui'
  )");
  ASSERT_TRUE(thread_slice.has_value()) << thread_slice.error();
  EXPECT_THAT(thread_slice.value(),
              ElementsAre(std::vector<std::string>{"name", "dur_us"},
                          std::vector<std::string>{"LiveThreadSlice", "5600"}));

  auto startup_args = QueryStartupArgs(test_trace_processor);
  ASSERT_TRUE(startup_args.has_value()) << startup_args.error();
  EXPECT_THAT(
      startup_args.value(),
      ElementsAre(std::vector<std::string>{"start_site", "finish_site", "mode"},
                  std::vector<std::string>{"WEBVIEW_INSTANCE_LOAD_URL",
                                           "ASYNC_WEBVIEW_STARTUP",
                                           "PARTIAL_ASYNC_THEN_SYNC"}));
}

TEST_F(TraceEventBindingTest,
       WebViewStartupFirstInstanceWithoutGlobalStartupAndArgs) {
  JNIEnv* env = AttachCurrentThread();
  base::test::TestTraceProcessor test_trace_processor;
  test_trace_processor.StartTrace("android_webview.timeline");
  Java_TraceEventNativeTestSupport_setEnabled(env, true);

  Java_TraceEventNativeTestSupport_webViewStartupFirstInstance(
      env, 210, 8, /*includedGlobalStartup=*/false);
  Java_TraceEventNativeTestSupport_webViewStartupStartChromiumLocked(
      env, 202, 7, static_cast<int>(webview_pb::GET_AW_TRACING_CONTROLLER),
      static_cast<int>(webview_pb::WEBVIEW_INSTANCE_LOAD_URL),
      static_cast<int>(webview_pb::FULLY_SYNC));

  absl::Status status = test_trace_processor.StopAndParseTrace();
  ASSERT_TRUE(status.ok()) << status.message();

  auto slices = QueryStartupSlices(test_trace_processor);
  ASSERT_TRUE(slices.has_value()) << slices.error();
  EXPECT_THAT(
      slices.value(),
      ElementsAre(
          std::vector<std::string>{"name", "ts_ms", "dur_ms", "track_name"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.StartChromiumLocked", "202", "7",
              "WebViewStartup"},
          std::vector<std::string>{
              "WebView.Startup.CreationTime.FirstInstanceWithoutGlobalStartup",
              "210", "8", "WebViewStartup"}));

  auto startup_args = QueryStartupArgs(test_trace_processor);
  ASSERT_TRUE(startup_args.has_value()) << startup_args.error();
  EXPECT_THAT(
      startup_args.value(),
      ElementsAre(
          std::vector<std::string>{"start_site", "finish_site", "mode"},
          std::vector<std::string>{"GET_AW_TRACING_CONTROLLER",
                                   "WEBVIEW_INSTANCE_LOAD_URL", "FULLY_SYNC"}));
}

}  // namespace

}  // namespace base::android

DEFINE_JNI(TraceEventNativeTestSupport)
