// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_WINDOW_INVOCATION_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_WINDOW_INVOCATION_TRACKER_H_

#include "chrome/browser/glic/public/glic_cui_tracker.h"

namespace glic {

// CUI 1: a window entry point (toolbar or tab strip button, OS hotkey or
// icon, 3-dot menu, ...) opening the panel. Records
// Glic.CUI.WindowEntryPointInvocation.*.
//
// Created by GlicInstanceCoordinatorImpl::Show() and handed to the
// invocation's GlicInvokeMetrics, which resolves it with the invocation's
// outcome (see GlicInvokeMetrics::SetCuiTracker()). It therefore agrees with
// the structured InvokeTerminated event by construction, rather than
// inferring an outcome from instance events.
class GlicWindowInvocationTracker : public GlicCuiTracker {
 public:
  GlicWindowInvocationTracker();
  ~GlicWindowInvocationTracker() override;

  GlicWindowInvocationTracker(const GlicWindowInvocationTracker&) = delete;
  GlicWindowInvocationTracker& operator=(const GlicWindowInvocationTracker&) =
      delete;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_WINDOW_INVOCATION_TRACKER_H_
