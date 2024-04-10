// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_
#define ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_

#include <optional>

#include "content/public/browser/tracing_delegate.h"

class PrefRegistrySimple;
namespace tracing {
class BackgroundTracingStateManager;
}

namespace android_webview {

class AwTracingDelegate : public content::TracingDelegate {
 public:
  AwTracingDelegate();
  explicit AwTracingDelegate(
      std::unique_ptr<tracing::BackgroundTracingStateManager> state_manager);
  ~AwTracingDelegate() override;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // content::TracingDelegate implementation:
  bool OnBackgroundTracingActive(bool requires_anonymized_data) override;
  bool OnBackgroundTracingIdle(bool requires_anonymized_data) override;

 private:
  bool IsAllowedToStartScenario() const;

  std::unique_ptr<tracing::BackgroundTracingStateManager> state_manager_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TRACING_AW_TRACING_DELEGATE_H_
