// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_config.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser_jni_headers/AwTracingController_jni.h"

using base::android::JavaRef;

namespace android_webview {

namespace {

// LINT.IfChange(WebViewApiCallTracingController)
enum class ApiCall {
  kTracingStart = 0,
  kTracingStartWithMemoryDump = 1,

  kMaxValue = kTracingStartWithMemoryDump
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:WebViewApiCallTracingController)

class AwTraceDataEndpoint
    : public content::TracingController::TraceDataEndpoint {
 public:
  using ReceivedChunkCallback =
      base::RepeatingCallback<void(std::unique_ptr<std::string>)>;

  static scoped_refptr<content::TracingController::TraceDataEndpoint> Create(
      ReceivedChunkCallback received_chunk_callback,
      base::OnceClosure completed_callback) {
    return new AwTraceDataEndpoint(std::move(received_chunk_callback),
                                   std::move(completed_callback));
  }

  void ReceivedTraceFinalContents() override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(completed_callback_));
  }

  void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) override {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(received_chunk_callback_, std::move(chunk)));
  }

  explicit AwTraceDataEndpoint(ReceivedChunkCallback received_chunk_callback,
                               base::OnceClosure completed_callback)
      : received_chunk_callback_(std::move(received_chunk_callback)),
        completed_callback_(std::move(completed_callback)) {}

  AwTraceDataEndpoint(const AwTraceDataEndpoint&) = delete;
  AwTraceDataEndpoint& operator=(const AwTraceDataEndpoint&) = delete;

 private:
  ~AwTraceDataEndpoint() override {}

  ReceivedChunkCallback received_chunk_callback_;
  base::OnceClosure completed_callback_;
};

void OnTraceDataComplete(const JavaObjectWeakGlobalRef& weak_java_object) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object.get(env);
  if (obj.obj()) {
    Java_AwTracingController_onTraceDataComplete(env, obj);
  }
}

void OnTraceDataReceived(const JavaObjectWeakGlobalRef& weak_java_object,
                         std::unique_ptr<std::string> chunk) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object.get(env);
  if (obj.obj()) {
    base::android::ScopedJavaLocalRef<jbyteArray> java_trace_data =
        base::android::ToJavaByteArray(env, *chunk);
    Java_AwTracingController_onTraceDataChunkReceived(env, obj,
                                                      java_trace_data);
  }
}

}  // namespace

static bool JNI_AwTracingController_Start(JNIEnv* env,
                                          const std::string& categories,
                                          int32_t jmode) {
  base::trace_event::TraceConfig trace_config(
      categories, static_cast<base::trace_event::TraceRecordMode>(jmode));

  UMA_HISTOGRAM_ENUMERATION("Android.WebView.ApiCall.TracingController",
                            ApiCall::kTracingStart);
  if (trace_config.IsCategoryGroupEnabled(
          base::trace_event::MemoryDumpManager::kTraceCategory)) {
    UMA_HISTOGRAM_ENUMERATION("Android.WebView.ApiCall.TracingController",
                              ApiCall::kTracingStartWithMemoryDump);
  }
  return content::TracingController::GetInstance()->StartTracing(
      trace_config, content::TracingController::StartTracingDoneCallback(),
      /*privacy_filtering_enabled=*/true);
}

static bool JNI_AwTracingController_StopAndFlush(
    JNIEnv* env,
    const JavaRef<jobject>& caller) {
  JavaObjectWeakGlobalRef weak_java_object(env, caller);
  // privacy_filtering_enabled=true is required for filtering out potential PII.
  return content::TracingController::GetInstance()->StopTracing(
      AwTraceDataEndpoint::Create(
          base::BindRepeating(&OnTraceDataReceived, weak_java_object),
          base::BindOnce(&OnTraceDataComplete, weak_java_object)),
      /*agent_label=*/"");
}

static bool JNI_AwTracingController_IsTracing(JNIEnv* env) {
  return content::TracingController::GetInstance()->IsTracing();
}

}  // namespace android_webview

DEFINE_JNI(AwTracingController)
