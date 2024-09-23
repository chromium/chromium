// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_RECORDER_H_
#define CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_RECORDER_H_

#include "components/metrics/structured/structured_metrics_recorder.h"

class PrefRegistrySimple;
class PrefService;

namespace metrics::structured {

// A recorder implementation for Chrome (Windows, Linux, and Mac).
//
// This implementation uses KeyDataProviderChrome and ChromeEventStorage for key
// and event handling. This recorder is fully initialized once it is created.
//
// This recorder only supports device events. If a profile event is recorded
// then it will be stored in-memory and never processed until Chrome is
// shutdown. This could cause an increase of memory consumption.
class ChromeStructuredMetricsRecorder final : public StructuredMetricsRecorder {
 public:
  explicit ChromeStructuredMetricsRecorder(PrefService* local_state);

  ChromeStructuredMetricsRecorder(const ChromeStructuredMetricsRecorder&) =
      delete;
  ChromeStructuredMetricsRecorder& operator=(
      const ChromeStructuredMetricsRecorder&) = delete;

  static void RegisterLocalState(PrefRegistrySimple* registry);

 private:
  friend class base::RefCountedDeleteOnSequence<StructuredMetricsRecorder>;
  friend class base::DeleteHelper<StructuredMetricsRecorder>;
  ~ChromeStructuredMetricsRecorder() override = default;
};

}  // namespace metrics::structured

#endif  // CHROME_BROWSER_METRICS_STRUCTURED_CHROME_STRUCTURED_METRICS_RECORDER_H_
