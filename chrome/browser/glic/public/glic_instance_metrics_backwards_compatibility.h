// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_

#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom.h"

namespace tabs {
class TabInterface;
}

namespace glic {

// Interface that's compatible for both `GlicInstanceMetrics` and
// `GlicMetrics`. This interface will be removed when the single-instance
// codepath is deprecated. Only intended for APIs that are either used in
// MultiInstance or in single-instance.
class GlicInstanceMetricsBackwardsCompatibility {
 public:
  virtual ~GlicInstanceMetricsBackwardsCompatibility() = default;

  virtual void OnUserInputSubmitted(mojom::WebClientMode mode) = 0;
  virtual void DidRequestContextFromTab(tabs::TabInterface& tab) = 0;
  virtual void OnResponseStarted() = 0;
  virtual void OnResponseStopped(mojom::ResponseStopCause cause) = 0;
  virtual void OnTurnCompleted(mojom::WebClientModel model,
                               base::TimeDelta duration) = 0;
  virtual void OnReaction(mojom::MetricUserInputReactionType reaction_type) = 0;

  // Called when glic requests a scroll.
  virtual void OnGlicScrollAttempt() = 0;

  // Called when scrolling starts (after glic requests to scroll) or if
  // the operation fails. `success` is true if a scroll was successfully
  // triggered.
  virtual void OnGlicScrollComplete(bool success) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_
