// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tab_observation_strategy.h"

#include <algorithm>
#include <utility>

#include "base/check.h"

namespace actor {

TabObservationStrategy::TabObservationStrategy() = default;

TabObservationStrategy::TabObservationStrategy(TabObservationStrategy&&) =
    default;

TabObservationStrategy& TabObservationStrategy::operator=(
    TabObservationStrategy&&) = default;

TabObservationStrategy::~TabObservationStrategy() = default;

void TabObservationStrategy::VoteForScreenshot(tabs::TabHandle tab,
                                               ScreenshotPolicy policy) {
  CHECK(!locked_);
  auto [it, inserted] = screenshot_votes_.emplace(tab, policy);
  if (!inserted) {
    it->second = std::max(it->second, policy);
  }
}

void TabObservationStrategy::VoteForPageContentExtraction(
    tabs::TabHandle tab,
    PageContentExtractionPolicy policy) {
  CHECK(!locked_);
  auto [it, inserted] = extraction_votes_.emplace(tab, policy);
  if (!inserted) {
    it->second = std::max(it->second, policy);
  }
}

void TabObservationStrategy::Lock() {
  locked_ = true;
}

ScreenshotPolicy TabObservationStrategy::GetScreenshotPolicy(
    tabs::TabHandle tab) const {
  CHECK(locked_);
  auto it = screenshot_votes_.find(tab);
  if (it != screenshot_votes_.end()) {
    return it->second;
  }
  return ScreenshotPolicy::kRequested;
}

PageContentExtractionPolicy
TabObservationStrategy::GetPageContentExtractionPolicy(
    tabs::TabHandle tab) const {
  CHECK(locked_);
  auto it = extraction_votes_.find(tab);
  if (it != extraction_votes_.end()) {
    return it->second;
  }
  return PageContentExtractionPolicy::kRequested;
}

}  // namespace actor
