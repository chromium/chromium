// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_ENTROPY_STATE_PROVIDER_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_ENTROPY_STATE_PROVIDER_H_

#include "base/memory/raw_ptr.h"
#include "components/metrics/entropy_state_provider.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics {
class SystemProfileProto;
}  // namespace metrics

namespace android_webview {

namespace prefs {
inline constexpr char kWebViewLowEntropySource[] = "webview.low_entropy_source";
}

// AwEntropyStateProvider adds information about low entropy sources in the
// system profile. In addition to the standard low entropy sources provided by
// `metrics::EntropyStateProvider`, this includes `webview_low_entropy_source`.
class AwEntropyStateProvider : public metrics::EntropyStateProvider {
 public:
  explicit AwEntropyStateProvider(PrefService* local_state);
  ~AwEntropyStateProvider() override;

  AwEntropyStateProvider(const AwEntropyStateProvider&) = delete;
  AwEntropyStateProvider& operator=(const AwEntropyStateProvider&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // metrics::EntropyStateProvider:
  void ProvideSystemProfileMetrics(
      metrics::SystemProfileProto* system_profile) override;

 private:
  raw_ptr<PrefService> pref_service_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_ENTROPY_STATE_PROVIDER_H_
