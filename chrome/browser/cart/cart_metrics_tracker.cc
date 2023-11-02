// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_metrics_tracker.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

CartMetricsTracker::CartMetricsTracker(Browser* browser) {
  if (!browser) {
    return;
  }
  browser->tab_strip_model()->AddObserver(this);
  BrowserList::GetInstance()->AddObserver(this);
}

CartMetricsTracker::~CartMetricsTracker() = default;

void CartMetricsTracker::ShutDown() {
  BrowserList::GetInstance()->RemoveObserver(this);
}

void CartMetricsTracker::PrepareToRecordUKM(const GURL& url) {
  last_interacted_url_ = url;
}

void CartMetricsTracker::TabChangedAt(content::WebContents* contents,
                                      int index,
                                      TabChangeType change_type) {
  if (change_type != TabChangeType::kAll) {
    return;
  }
  if (last_interacted_url_) {
    if (last_interacted_url_ == contents->GetVisibleURL()) {
      ukm::builders::Shopping_ChromeCart(
          contents->GetPrimaryMainFrame()->GetPageUkmSourceId())
          .SetVisitCart(true)
          .Record(ukm::UkmRecorder::Get());
    }
    last_interacted_url_.reset();
  }
}

void CartMetricsTracker::OnBrowserAdded(Browser* browser) {
  browser->tab_strip_model()->AddObserver(this);
}

void CartMetricsTracker::OnBrowserRemoved(Browser* browser) {
  browser->tab_strip_model()->RemoveObserver(this);
}
