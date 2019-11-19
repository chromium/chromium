// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace page_load_metrics {

PageLoadMetricsObserverTestHarness::PageLoadMetricsObserverTestHarness()
    : ChromeRenderViewHostTestHarness() {}

PageLoadMetricsObserverTestHarness::~PageLoadMetricsObserverTestHarness() {}

void PageLoadMetricsObserverTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("http://www.google.com"));
  // Page load metrics depends on UKM source URLs being recorded, so make sure
  // the SourceUrlRecorderWebContentsObserver is instantiated.
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  tester_ = std::make_unique<PageLoadMetricsObserverTester>(
      web_contents(), this,
      base::BindRepeating(
          &PageLoadMetricsObserverTestHarness::RegisterObservers,
          base::Unretained(this)));
  web_contents()->WasShown();
}

const char PageLoadMetricsObserverTestHarness::kResourceUrl[] =
    "https://www.example.com/resource";

}  // namespace page_load_metrics
