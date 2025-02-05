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
  GlicMetrics();
  GlicMetrics(const GlicMetrics&) = delete;
  GlicMetrics& operator=(const GlicMetrics&) = delete;
  ~GlicMetrics();

  // See glic.mojom for details. These are events from the web client. The
  // lifetime of the web client is scoped to that of the window, so if these
  // methods are called then controller_ is guaranteed to exist.
  void OnUserInputSubmitted(mojom::WebClientMode mode);
  void OnResponseStarted();
  void OnResponseStopped();
  void OnSessionTerminated();
  void OnResponseRated(bool positive);

  // Public API called by other glic classes.
  // Called when the glic window starts to open.
  void OnGlicWindowOpen();
  // Called when the glic window finishes closing.
  void OnGlicWindowClose();

  // Must be called immediately after constructor before any calls from
  // glic.mojom.
  void SetWindowController(GlicWindowController* controller);

 private:
  // These members are cleared in OnResponseStopped.
  base::TimeTicks input_submitted_time_;
  mojom::WebClientMode input_mode_;
  base::TimeTicks response_started_time_;

  // Cleared in OnGlicWindowClose.
  int session_responses_ = 0;
  base::TimeTicks session_start_time_;

  // The owner of this class is responsible for maintaining appropriate lifetime
  // for controller_.
  raw_ptr<GlicWindowController> controller_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
