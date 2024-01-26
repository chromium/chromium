// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/chrome_structured_metrics_recorder.h"

#include <memory>

#include "chrome/browser/metrics/structured/chrome_event_storage.h"
#include "chrome/browser/metrics/structured/key_data_provider_chrome.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics::structured {

ChromeStructuredMetricsRecorder::ChromeStructuredMetricsRecorder(
    PrefService* local_state)
    : StructuredMetricsRecorder(
          std::make_unique<KeyDataProviderChrome>(local_state),
          std::make_unique<ChromeEventStorage>()) {
  // If KeyDataProviderChrome becomes async, then NotifyKeyReady needs to be
  // removed and the KeyDataProvider must notify the recorder when the keys are
  // ready.
  key_data_provider()->NotifyKeyReady();
}

// static:
void ChromeStructuredMetricsRecorder::RegisterLocalState(
    PrefRegistrySimple* registry) {
  KeyDataProviderChrome::RegisterLocalState(registry);
}

}  // namespace metrics::structured
