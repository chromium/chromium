// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_PERF_TRAITS_TRACKER_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_PERF_TRAITS_TRACKER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"

namespace content {
class WebContents;
}

namespace glic {

using GlicActuationState = performance_manager::GlicActuationState;

// Tracks the performance and priority-related traits of Glic.
// Observed by Performance Manager's PageLiveStateDecoratorHelper on the UI
// thread.
class GlicPerfTraitsTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnGlicActuationStateChanged(content::WebContents* w,
                                             GlicActuationState state) {}
    virtual void OnIsGlicPinnedToVisibleInstanceChanged(
        content::WebContents* w,
        bool is_pinned_to_visible) {}
  };

  static GlicPerfTraitsTracker* GetInstance();

  GlicPerfTraitsTracker();
  ~GlicPerfTraitsTracker();
  GlicPerfTraitsTracker(const GlicPerfTraitsTracker&) = delete;
  GlicPerfTraitsTracker& operator=(const GlicPerfTraitsTracker&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void NotifyActuationStateChanged(content::WebContents* web_contents,
                                   GlicActuationState state);

  void NotifyIsGlicPinnedToVisibleInstanceChanged(
      content::WebContents* web_contents,
      bool is_pinned_to_visible);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_PERF_TRAITS_TRACKER_H_
