// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/aw_entropy_state_provider.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace android_webview {

namespace {

// Default value indicating the low entropy source has not been set.
constexpr int kLowEntropySourceNotSet = -1;

}  // namespace

AwEntropyStateProvider::AwEntropyStateProvider(PrefService* local_state)
    : metrics::EntropyStateProvider(local_state), pref_service_(local_state) {}

AwEntropyStateProvider::~AwEntropyStateProvider() = default;

// static
void AwEntropyStateProvider::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kWebViewLowEntropySource,
                                kLowEntropySourceNotSet);
}

void AwEntropyStateProvider::ProvideSystemProfileMetrics(
    metrics::SystemProfileProto* system_profile) {
  metrics::EntropyStateProvider::ProvideSystemProfileMetrics(system_profile);
  if (pref_service_) {
    int value = pref_service_->GetInteger(prefs::kWebViewLowEntropySource);
    if (value >= 0) {
      system_profile->set_webview_low_entropy_source(value);
    }
  }
}

}  // namespace android_webview
