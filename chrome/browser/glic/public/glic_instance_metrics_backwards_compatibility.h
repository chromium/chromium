// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_

#include "chrome/browser/glic/host/glic.mojom.h"

namespace glic {

// Interface that's compatible for both `GlicInstanceMetrics` and
// `GlicMetrics`. This interface will be removed when the single-instance
// codepath is deprecated. Only intended for APIs that are either used in
// MultiInstance or in single-instance.
class GlicInstanceMetricsBackwardsCompatibility {
 public:
  virtual ~GlicInstanceMetricsBackwardsCompatibility() = default;

  // Called when glic requests a scroll.
  virtual void OnGlicScrollAttempt() = 0;

  // Called when scrolling starts (after glic requests to scroll) or if
  // the operation fails. `success` is true if a scroll was successfully
  // triggered.
  virtual void OnGlicScrollComplete(bool success) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_
