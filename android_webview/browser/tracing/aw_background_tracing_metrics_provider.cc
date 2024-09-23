// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_background_tracing_metrics_provider.h"

#include <string_view>

#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/thread_pool.h"
#include "components/metrics/field_trials_provider.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/version_utils.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/version_info/android/channel_getter.h"
#include "services/tracing/public/cpp/trace_startup_config.h"
#include "third_party/metrics_proto/trace_log.pb.h"
#include "third_party/zlib/google/compression_utils.h"

namespace tracing {

AwBackgroundTracingMetricsProvider::AwBackgroundTracingMetricsProvider() =
    default;
AwBackgroundTracingMetricsProvider::~AwBackgroundTracingMetricsProvider() =
    default;

void AwBackgroundTracingMetricsProvider::DoInit() {
  tracing::TraceStartupConfig::GetInstance().SetBackgroundStartupTracingEnabled(
      tracing::ShouldTraceStartup());
  SetupFieldTracingFromFieldTrial();

  metrics::MetricsService* metrics =
      android_webview::AwMetricsServiceClient::GetInstance()
          ->GetMetricsService();
  DCHECK(metrics);

  system_profile_providers_.emplace_back(
      std::make_unique<variations::FieldTrialsProvider>(
          metrics->GetSyntheticTrialRegistry(), std::string_view()));
}

base::OnceCallback<bool(metrics::ChromeUserMetricsExtension*, std::string&&)>
AwBackgroundTracingMetricsProvider::GetEmbedderMetricsProvider() {
  return base::BindOnce([](metrics::ChromeUserMetricsExtension* uma_proto,
                           std::string&& compressed_trace) {
    if (compressed_trace.size() > kCompressedUploadLimitBytes) {
      return false;
    }
    SetTrace(uma_proto->add_trace_log(), std::move(compressed_trace));

    // Remove the package name according to the privacy requirements.
    // See go/public-webview-trace-collection.
    auto* system_profile = uma_proto->mutable_system_profile();
    system_profile->clear_app_package_name();
    return true;
  });
}

void AwBackgroundTracingMetricsProvider::RecordCoreSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile_proto) {
  metrics::MetricsLog::RecordCoreSystemProfile(
      metrics::GetVersionString(),
      metrics::AsProtobufChannel(version_info::android::GetChannel()), false,
      base::i18n::GetConfiguredLocale(), std::string(), system_profile_proto);
}

}  // namespace tracing
