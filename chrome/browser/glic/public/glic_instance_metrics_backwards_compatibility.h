// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_

#include "base/time/time.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"

namespace glic {

// Per-instance metrics APIs that `Host` forwards to the instance's metrics
// recorder. Originally shared with the single-instance `GlicMetrics`; now
// only implemented by `GlicInstanceMetrics`.
// TODO(b/468097191): Fold this interface into `GlicInstanceMetrics`.
class GlicInstanceMetricsBackwardsCompatibility {
 public:
  virtual ~GlicInstanceMetricsBackwardsCompatibility() = default;

  virtual void OnUserInputSubmitted(mojom::WebClientMode mode,
                                    mojom::PromptType prompt_type) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_INSTANCE_METRICS_BACKWARDS_COMPATIBILITY_H_
