// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/glic.mojom.h"
#include "chrome/browser/glic/glic_enums.h"

class Profile;

namespace glic {

class GlicWindowController;

// Responsible for all glic web-client metrics, and all stateful glic metrics.
// Some stateless glic metrics are logged inline in the relevant code for
// convenience.
class GlicMetrics {
 public:
  explicit GlicMetrics(Profile* profile);
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

  // ----Public API called by other glic classes-----
  // Called when the glic window starts to open.
  void OnGlicWindowOpen(bool attached, InvocationSource source);
  // Called when the glic window finishes closing.
  void OnGlicWindowClose();
  // Called when the glic window attaches or detaches.

  // Must be called immediately after constructor before any calls from
  // glic.mojom.
  void SetWindowController(GlicWindowController* controller);

 private:
  // Called when `impression_timer_` fires.
  void OnImpressionTimerFired();

  // These members are cleared in OnResponseStopped.
  base::TimeTicks input_submitted_time_;
  mojom::WebClientMode input_mode_;
  base::TimeTicks response_started_time_;

  // Session state. `session_start_time_` is a sentinel that is cleared in
  // OnGlicWindowClose() and is used to determine whether OnGlicWindowOpen was
  // called.
  int session_responses_ = 0;
  base::TimeTicks session_start_time_;
  InvocationSource invocation_source_ = InvocationSource::kOsButton;

  // Used to record impressions of glic entry points.
  base::RepeatingTimer impression_timer_;

  // The owner of this class is responsible for maintaining appropriate lifetime
  // for controller_.
  raw_ptr<GlicWindowController> controller_;
  raw_ptr<Profile> profile_;
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_H_
