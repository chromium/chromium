// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/tab_usage_scenario_tracker.h"

#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"

namespace metrics {

TabUsageScenarioTracker::TabUsageScenarioTracker(
    UsageScenarioDataStoreImpl* usage_scenario_data_store)
    : usage_scenario_data_store_(usage_scenario_data_store) {
  // TODO(crbug.com/1153193): Owners of this class have to set the initial
  // state. Constructing the object like this starts off the state as empty. If
  // tabs/windows already exist when this object is created they need to be
  // added using the normal functions after creation.
}
TabUsageScenarioTracker::~TabUsageScenarioTracker() = default;

void TabUsageScenarioTracker::OnTabAdded(content::WebContents* web_contents) {
  OnTabAdded(web_contents, web_contents->GetVisibility());
}

void TabUsageScenarioTracker::OnTabAddedForTesting(
    content::WebContents* web_contents,
    content::Visibility initial_visibility) {
  OnTabAdded(web_contents, initial_visibility);
}

void TabUsageScenarioTracker::OnTabAdded(
    content::WebContents* web_contents,
    content::Visibility initial_visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnTabAdded();

  // Tab is added already visible. It will not get a separate visibility update
  // so we handle the visibility here.
  if (web_contents->GetVisibility() == content::Visibility::VISIBLE) {
    // If web-content was not already reported as visible notify data store.
    if (visible_contents_.insert(web_contents).second) {
      usage_scenario_data_store_->OnWindowVisible();
    }
  }
}

void TabUsageScenarioTracker::OnTabRemoved(content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnTabClosed();
}

void TabUsageScenarioTracker::OnTabVisibilityChanged(
    content::WebContents* web_contents,
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (visibility == content::Visibility::VISIBLE) {
    // If web-content was not already reported as visible notify data store.
    if (visible_contents_.insert(web_contents).second) {
      usage_scenario_data_store_->OnWindowVisible();
    }
  } else {
    visible_contents_.erase(web_contents);
    usage_scenario_data_store_->OnWindowHidden();
  }
}

void TabUsageScenarioTracker::OnTabInteraction(
    content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_scenario_data_store_->OnUserInteraction();
}

}  // namespace metrics
