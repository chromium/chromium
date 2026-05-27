// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/android_metrics_log_uploader.h"

#include "base/android/jni_array.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "components/metrics/log_decoder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/browser/metrics/aw_histograms_allowlist.h"
#include "android_webview/browser_jni_headers/AndroidMetricsLogUploader_jni.h"
#include "android_webview/common/aw_features.h"
#include "base/feature_list.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"

using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace metrics {

AndroidMetricsLogUploader::AndroidMetricsLogUploader(
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : on_upload_complete_(on_upload_complete) {}

AndroidMetricsLogUploader::~AndroidMetricsLogUploader() = default;

int32_t UploadLogWithUploader(std::string log_data) {
  if (base::FeatureList::IsEnabled(
          android_webview::features::kWebViewCppMetricsFiltering)) {
    AndroidMetricsLogUploader::MaybeFilterLog(log_data);
  }
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> java_data = ToJavaByteArray(env, log_data);

  return Java_AndroidMetricsLogUploader_uploadLog(env, java_data);
}

void AndroidMetricsLogUploader::MaybeFilterLog(std::string& log_data) {
  metrics::ChromeUserMetricsExtension proto;
  if (!proto.ParseFromString(log_data)) {
    LOG(WARNING) << "Failed to parse ChromeUserMetricsExtension proto in C++";
    return;
  }

  if (!(proto.has_system_profile() &&
        proto.system_profile().has_metrics_filtering_status() &&
        proto.system_profile().metrics_filtering_status() ==
            metrics::SystemProfileProto::METRICS_ONLY_CRITICAL)) {
    return;
  }

  proto.clear_user_action_event();

  auto* mutable_histograms = proto.mutable_histogram_event();
  auto* allowlist = android_webview::AwHistogramsAllowlist::GetInstance();
  int write_index = 0;
  for (int read_index = 0; read_index < mutable_histograms->size();
       ++read_index) {
    auto* event = mutable_histograms->Mutable(read_index);
    if (allowlist->Contains(event->name_hash())) {
      if (write_index != read_index) {
        mutable_histograms->SwapElements(write_index, read_index);
      }
      write_index++;
    }
  }
  mutable_histograms->DeleteSubrange(write_index,
                                     mutable_histograms->size() - write_index);

  proto.SerializeToString(&log_data);
}

void AndroidMetricsLogUploader::UploadLog(
    const std::string& compressed_log_data,
    const LogMetadata& /*log_metadata*/,
    const std::string& /*log_hash*/,
    const std::string& /*log_signature*/,
    const ReportingInfo& reporting_info) {
  // This uploader uses the platform logging mechanism instead of the normal UMA
  // server. The platform mechanism does its own compression, so undo the
  // previous compression.
  std::string log_data;
  if (!DecodeLogData(compressed_log_data, &log_data)) {
    // If the log is corrupt, pretend the server rejected it (HTTP Bad Request).
    OnUploadComplete(400);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&UploadLogWithUploader, log_data),
      base::BindOnce(&AndroidMetricsLogUploader::OnUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void AndroidMetricsLogUploader::OnUploadComplete(const int32_t status) {
  on_upload_complete_.Run(status, /*error_code=*/0, /*was_https=*/true,
                          /*force_discard=*/false, /*force_discard_reason=*/"");
}

}  // namespace metrics

DEFINE_JNI(AndroidMetricsLogUploader)
