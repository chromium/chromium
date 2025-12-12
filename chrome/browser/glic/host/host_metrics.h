// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_HOST_METRICS_H_
#define CHROME_BROWSER_GLIC_HOST_HOST_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"

namespace glic {

class Host;

// Tracks and records metrics related to the Glic Host.
//
// This class is extracted from `Host` to encapsulate metrics logic and state,
// preventing `Host` from becoming cluttered with metrics-specific
// implementation details.
//
// Metrics tracked:
// *   Periodic private memory footprint of the WebUI and WebClient render
//     processes.
// *   Maximum private memory footprint observed during the session for both
//     WebUI and WebClient.
class HostMetrics {
 public:
  explicit HostMetrics(Host* host);
  ~HostMetrics();

  void StartRecording();
  void Shutdown();

 private:
  void RecordMemoryMetrics();
  void OnPrivateMemoryFootprint(bool is_webui, uint64_t bytes);

  raw_ptr<Host> host_;
  base::RepeatingTimer memory_metrics_timer_;
  uint64_t max_webui_memory_ = 0;
  uint64_t max_web_client_memory_ = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_HOST_METRICS_H_
