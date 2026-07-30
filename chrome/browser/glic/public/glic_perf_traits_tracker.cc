// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/public/glic_perf_traits_tracker.h"

#include "base/no_destructor.h"
#include "content/public/browser/web_contents.h"

namespace glic {

// static
GlicPerfTraitsTracker* GlicPerfTraitsTracker::GetInstance() {
  static base::NoDestructor<GlicPerfTraitsTracker> instance;
  return instance.get();
}

GlicPerfTraitsTracker::GlicPerfTraitsTracker() = default;

GlicPerfTraitsTracker::~GlicPerfTraitsTracker() = default;

void GlicPerfTraitsTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void GlicPerfTraitsTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void GlicPerfTraitsTracker::NotifyActuationStateChanged(
    content::WebContents* web_contents,
    GlicActuationState state) {
  for (auto& observer : observers_) {
    observer.OnGlicActuationStateChanged(web_contents, state);
  }
}

void GlicPerfTraitsTracker::NotifyIsGlicPinnedToVisibleInstanceChanged(
    content::WebContents* web_contents,
    bool is_pinned_to_visible) {
  for (auto& observer : observers_) {
    observer.OnIsGlicPinnedToVisibleInstanceChanged(web_contents,
                                                    is_pinned_to_visible);
  }
}

}  // namespace glic
