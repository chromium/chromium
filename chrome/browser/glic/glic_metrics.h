// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_H_

#include "base/time/time.h"
#include "chrome/browser/glic/glic.mojom.h"

namespace glic {

class GlicWindowController;

// Responsible for all glic web-client metrics, and all stateful glic metrics.
// Some stateless glic metrics are logged inline in the relevant code for
// convenience.
class GlicMetrics {
 public:
  explicit GlicMetrics(GlicWindowController* window_controller);
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics();

  // See glic.mojom for details.
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void OnResponseStarted();
  void OnResponseStopped();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);

 private:
  // These members are cleared in OnResponseStopped.
  base::TimeTicks input_submitted_time_;
  mojom::WebClientMode input_mode_;
  base::TimeTicks response_started_time_;

  // Guaranteed to outlive `this`.
  raw_ptr<GlicWindowController> window_controller_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
