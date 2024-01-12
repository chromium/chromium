// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/page_load_metrics_observer_test_harness.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace page_load_metrics {

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
          base::Unretained(this)),
      IsNonTabWebUI());
  web_contents()->WasShown();
}

bool PageLoadMetricsObserverTestHarness::IsNonTabWebUI() const {
  return false;
}

void PageLoadMetricsObserverTestHarness::InitializeFeatureList() {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {
          {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
      },
      {
          // Disable the memory requirement of Prerender2
          // so the test can run on any bot.
          {blink::features::kPrerender2MemoryControls},
      });
}

const char PageLoadMetricsObserverTestHarness::kResourceUrl[] =
    "https://www.example.com/resource";

}  // namespace page_load_metrics
