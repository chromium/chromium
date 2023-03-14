// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler_base.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"

namespace metrics {

FirstWebContentsProfilerBase::FirstWebContentsProfilerBase(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

FirstWebContentsProfilerBase::~FirstWebContentsProfilerBase() = default;

// static
content::WebContents* FirstWebContentsProfilerBase::GetVisibleContents(
    Browser* browser) {
  if (!browser->window()->IsVisible())
    return nullptr;

  // The active WebContents may be hidden when the window height is small.
  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();

  // It is incorrect to have a visible browser window with no active
  // WebContents, but reports on show that it happens.
  // See https://crbug.com/1032348 for Mac or https://crbug.com/1414831 for Win.
  if (!contents)
    return nullptr;

  if (contents->GetVisibility() != content::Visibility::VISIBLE)
    return nullptr;

  return contents;
}

void FirstWebContentsProfilerBase::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // The profiler is concerned with the primary main frame navigation only.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // The profiler is created after DidStartNavigation() has been dispatched for
  // the first top-level navigation. If another DidStartNavigation() is
  // received, it means that a new navigation was initiated.
  FinishedCollectingMetrics(
      StartupProfilingFinishReason::kAbandonNewNavigation);
}

void FirstWebContentsProfilerBase::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (WasStartupInterrupted()) {
    FinishedCollectingMetrics(StartupProfilingFinishReason::kAbandonBlockingUI);
    return;
  }

  // Ignore subframe navigations, pre-rendering, and same-document navigations.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted() || navigation_handle->IsErrorPage()) {
    FinishedCollectingMetrics(
        StartupProfilingFinishReason::kAbandonNavigationError);
    return;
  }

  // It is not possible to get a second top-level DidFinishNavigation() without
  // first having a DidStartNavigation(), which would have deleted |this|.
  DCHECK(!did_finish_first_navigation_);

  did_finish_first_navigation_ = true;

  RecordNavigationFinished(navigation_handle->NavigationStart());
}

void FirstWebContentsProfilerBase::DidFirstVisuallyNonEmptyPaint() {
  DCHECK(did_finish_first_navigation_);

  if (WasStartupInterrupted()) {
    FinishedCollectingMetrics(StartupProfilingFinishReason::kAbandonBlockingUI);
    return;
  }

  RecordFirstNonEmptyPaint();
  FinishedCollectingMetrics(StartupProfilingFinishReason::kDone);
}

void FirstWebContentsProfilerBase::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility != content::Visibility::VISIBLE) {
    // Stop profiling if the content gets hidden as its load may be
    // deprioritized and timing it becomes meaningless.
    FinishedCollectingMetrics(
        StartupProfilingFinishReason::kAbandonContentHidden);
  }
}

void FirstWebContentsProfilerBase::WebContentsDestroyed() {
  FinishedCollectingMetrics(
      StartupProfilingFinishReason::kAbandonContentDestroyed);
}

void FirstWebContentsProfilerBase::FinishedCollectingMetrics(
    StartupProfilingFinishReason finish_reason) {
  RecordFinishReason(finish_reason);
  delete this;
}

}  // namespace metrics
