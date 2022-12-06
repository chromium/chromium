// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_

#include "content/public/browser/tracing_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefRegistrySimple;

namespace base {
class Value;
}  // namespace base

namespace android_webview {

class AwTracingDelegate : public content::TracingDelegate {
 public:
  AwTracingDelegate();
  ~AwTracingDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // content::TracingDelegate implementation:
  bool IsAllowedToBeginBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data) override;
  bool IsAllowedToEndBackgroundScenario(
      const content::BackgroundTracingConfig& config,
      bool requires_anonymized_data,
      bool is_crash_scenario) override;
  absl::optional<base::Value> GenerateMetadataDict() override;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_
