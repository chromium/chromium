// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_window_tracker.h"

#include <memory>
#include <string_view>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"

namespace android {

namespace {

const void* const kWindowCountUserDataKey = &kWindowCountUserDataKey;

std::string_view GetExistingWindowVariant(bool had_existing_window) {
  return had_existing_window ? "WithExistingWindow" : "WithoutExistingWindow";
}

void RecordWebUiStartupPhase(std::string_view phase,
                             base::TimeDelta duration,
                             bool had_existing_window) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"Android.TabStrip.NewTabButton.WebUI.", phase}), duration);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Android.TabStrip.NewTabButton.WebUI.",
                    GetExistingWindowVariant(had_existing_window), ".", phase}),
      duration);
}

struct WindowCount : public base::SupportsUserData::Data {
  int count = 0;
};

WindowCount& GetWindowCount(Profile* profile) {
  auto* data =
      static_cast<WindowCount*>(profile->GetUserData(kWindowCountUserDataKey));
  if (!data) {
    auto owned = std::make_unique<WindowCount>();
    data = owned.get();
    profile->SetUserData(kWindowCountUserDataKey, std::move(owned));
  }
  return *data;
}

}  // namespace

TabStripWindowTracker::TabStripWindowTracker(Profile* profile)
    : profile_(profile->GetWeakPtr()),
      had_existing_window_(GetWindowCount(profile).count++ > 0) {}

TabStripWindowTracker::~TabStripWindowTracker() {
  // If the Profile is gone, its count went with it; nothing to undo.
  if (!profile_) {
    return;
  }
  WindowCount& window_count = GetWindowCount(profile_.get());
  CHECK_GT(window_count.count, 0);
  --window_count.count;
}

void RecordNewTabButtonActivityCreationToFirstPaint(base::TimeDelta duration,
                                                    bool had_existing_window) {
  base::UmaHistogramMediumTimes(
      "Android.TabStrip.NewTabButton.ActivityCreationToFirstPaint", duration);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Android.TabStrip.NewTabButton.",
                    GetExistingWindowVariant(had_existing_window),
                    ".ActivityCreationToFirstPaint"}),
      duration);
}

void RecordNewTabButtonWebUiStartupPhases(
    base::TimeDelta navigation_start_to_commit,
    base::TimeDelta commit_to_dom_content_loaded,
    base::TimeDelta dom_content_loaded_to_first_paint,
    bool had_existing_window) {
  RecordWebUiStartupPhase("NavigationStartToCommit", navigation_start_to_commit,
                          had_existing_window);
  RecordWebUiStartupPhase("CommitToDOMContentLoaded",
                          commit_to_dom_content_loaded, had_existing_window);
  RecordWebUiStartupPhase("DOMContentLoadedToFirstPaint",
                          dom_content_loaded_to_first_paint,
                          had_existing_window);
}

}  // namespace android
