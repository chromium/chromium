// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/browser_window_histogram_helper.h"

#include "components/startup_metric_utils/browser/startup_metric_utils.h"

BrowserWindowHistogramHelper::~BrowserWindowHistogramHelper() {}

// static
std::unique_ptr<BrowserWindowHistogramHelper>
BrowserWindowHistogramHelper::MaybeRecordValueAndCreateInstanceOnBrowserPaint(
    ui::Compositor* compositor) {
  static bool did_first_paint = false;
  if (did_first_paint)
    return nullptr;

  did_first_paint = true;

  return std::unique_ptr<BrowserWindowHistogramHelper>(
      new BrowserWindowHistogramHelper(compositor));
}

BrowserWindowHistogramHelper::BrowserWindowHistogramHelper(
    ui::Compositor* compositor) {
  startup_metric_utils::RecordBrowserWindowFirstPaint(base::TimeTicks::Now());

#if defined(OS_MACOSX)
  if (!compositor) {
    // In Cocoa version of Chromium, UI is rendered inside the main process
    // using CoreAnimation compositor, and at this point everything is already
    // visible to the user.
    startup_metric_utils::RecordBrowserWindowFirstPaintCompositingEnded(
        base::TimeTicks::Now());
    return;
  }
#endif  // OS_MACOSX

  scoped_observer_.Add(compositor);
}

void BrowserWindowHistogramHelper::OnCompositingEnded(
    ui::Compositor* compositor) {
  startup_metric_utils::RecordBrowserWindowFirstPaintCompositingEnded(
      base::TimeTicks::Now());

  scoped_observer_.RemoveAll();
}

void BrowserWindowHistogramHelper::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  scoped_observer_.RemoveAll();
}
