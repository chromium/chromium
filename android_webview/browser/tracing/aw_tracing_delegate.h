// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "content/public/browser/tracing_delegate.h"

class PrefRegistrySimple;
class PrefService;
namespace tracing {
class BackgroundTracingStateManager;
}

namespace android_webview {

class AwTracingDelegate : public content::TracingDelegate {
 public:
  explicit AwTracingDelegate(PrefService& local_state);
  ~AwTracingDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // content::TracingDelegate implementation:
  bool IsRecordingAllowed(IsLocalScenario is_local_scenario,
                          base::TimeTicks session_start) const override;
  std::unique_ptr<tracing::BackgroundTracingStateManager> CreateStateManager()
      override;
  std::string RecordSerializedSystemProfileMetrics() const override;
  tracing::MetadataDataSource::BundleRecorder
  CreateSystemProfileMetadataRecorder() const override;
  tracing::MetadataDataSource::ChromeMetadataRecorder
  CreateChromeMetadataPacketRecorder() const override;

 private:
  const raw_ref<PrefService> local_state_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_
