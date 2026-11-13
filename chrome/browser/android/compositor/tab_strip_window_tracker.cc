// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/compositor/tab_strip_window_tracker.h"

#include <memory>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"

namespace android {

namespace {

const void* const kWindowCountUserDataKey = &kWindowCountUserDataKey;

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
      base::StrCat(
          {"Android.TabStrip.NewTabButton.",
           had_existing_window ? "WithExistingWindow" : "WithoutExistingWindow",
           ".ActivityCreationToFirstPaint"}),
      duration);
}

}  // namespace android
