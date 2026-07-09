// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_WINDOW_INVOCATION_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_WINDOW_INVOCATION_TRACKER_H_

#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_cui_tracker.h"

namespace glic {

class GlicWindowInvocationTracker : public GlicCuiTracker {
 public:
  GlicWindowInvocationTracker();
  ~GlicWindowInvocationTracker() override;

  GlicWindowInvocationTracker(const GlicWindowInvocationTracker&) = delete;
  GlicWindowInvocationTracker& operator=(const GlicWindowInvocationTracker&) =
      delete;

 protected:
  const char* GetMetricName() const override;
  std::optional<GlicCuiOutcome> GetEventOutcome(
      GlicInstanceEvent event) const override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_WINDOW_INVOCATION_TRACKER_H_
