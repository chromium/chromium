// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_BROWSER_WINDOW_HISTOGRAM_HELPER_H_
#define CHROME_BROWSER_METRICS_BROWSER_WINDOW_HISTOGRAM_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/compositor_observer.h"

// Class that encapsulates logic of recording
// Startup.BrowserWindow.FirstPaint* histograms.
//
// There's a dependency on ui/compositor therefore it can't be moved to
// components/startup_metrics_utils.
class BrowserWindowHistogramHelper : public ui::CompositorObserver {
 public:
  ~BrowserWindowHistogramHelper() override;

  // Call this when the Browser finishes painting its UI, and the user will see
  // it after next Compositor frame swap.
  // |compositor| is the compositor that composites the just-painted Browser
  // widget, or nullptr in Cocoa (we're using CoreAnimation's compositor there).
  // Returned object should stay alive until next |OnCompositingStarted|
  // callback.
  static std::unique_ptr<BrowserWindowHistogramHelper>
  MaybeRecordValueAndCreateInstanceOnBrowserPaint(ui::Compositor* compositor);

 private:
  explicit BrowserWindowHistogramHelper(ui::Compositor* compositor);

  // ui::CompositorObserver:
  void OnCompositingEnded(ui::Compositor* compositor) override;
  void OnCompositingShuttingDown(ui::Compositor* compositor) override;

  ScopedObserver<ui::Compositor, ui::CompositorObserver> scoped_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserWindowHistogramHelper);
};

#endif  // CHROME_BROWSER_METRICS_BROWSER_WINDOW_HISTOGRAM_HELPER_H_
