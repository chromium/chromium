// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/tracing/aw_tracing_delegate.h"

#include <memory>

#include "android_webview/browser/aw_browser_process.h"
#include "components/tracing/common/background_tracing_metrics_provider.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/pref_names.h"
#include "components/tracing/common/system_profile_metadata_recorder.h"

namespace android_webview {

AwTracingDelegate::AwTracingDelegate() = default;
AwTracingDelegate::~AwTracingDelegate() = default;

// static
void AwTracingDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(tracing::kBackgroundTracingSessionState);
}

bool AwTracingDelegate::IsRecordingAllowed(
    bool requires_anonymized_data,
    base::TimeTicks session_start) const {
  return true;
}

std::unique_ptr<tracing::BackgroundTracingStateManager>
AwTracingDelegate::CreateStateManager() {
  return tracing::BackgroundTracingStateManager::CreateInstance(
      AwBrowserProcess::GetInstance()->local_state());
}

std::string AwTracingDelegate::RecordSerializedSystemProfileMetrics() const {
  metrics::SystemProfileProto system_profile_proto;
  auto recorder = tracing::BackgroundTracingMetricsProvider::
      GetSystemProfileMetricsRecorder();
  if (!recorder) {
    return std::string();
  }
  recorder.Run(system_profile_proto);
  std::string serialized_system_profile;
  system_profile_proto.SerializeToString(&serialized_system_profile);
  return serialized_system_profile;
}

tracing::MetadataDataSource::BundleRecorder
AwTracingDelegate::CreateSystemProfileMetadataRecorder() const {
  return base::BindRepeating(&tracing::RecordSystemProfileMetadata);
}

}  // namespace android_webview
