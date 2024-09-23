// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/first_web_contents_profiler.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/first_web_contents_profiler_base.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace metrics {
namespace {

void RecordFirstWebContentsFinishReason(
    StartupProfilingFinishReason finish_reason) {
  base::UmaHistogramEnumeration("Startup.FirstWebContents.FinishReason",
                                finish_reason);
}

class FirstWebContentsProfiler : public FirstWebContentsProfilerBase {
 public:
  explicit FirstWebContentsProfiler(content::WebContents* web_contents);

  FirstWebContentsProfiler(const FirstWebContentsProfiler&) = delete;
  FirstWebContentsProfiler& operator=(const FirstWebContentsProfiler&) = delete;

 protected:
  // FirstWebContentsProfilerBase:
  void RecordFinishReason(StartupProfilingFinishReason finish_reason) override;
  void RecordNavigationFinished(base::TimeTicks navigation_start) override;
  void RecordFirstNonEmptyPaint() override;
  bool WasStartupInterrupted() override;

 private:
  ~FirstWebContentsProfiler() override = default;
};

FirstWebContentsProfiler::FirstWebContentsProfiler(
    content::WebContents* web_contents)
    : FirstWebContentsProfilerBase(web_contents) {
  // FirstWebContentsProfiler is created before the main MessageLoop starts
  // running. At that time, any visible WebContents should have a pending
  // NavigationEntry, i.e. should have dispatched DidStartNavigation() but not
  // DidFinishNavigation().
  DCHECK(web_contents->GetController().GetPendingEntry());
}

void FirstWebContentsProfiler::RecordFinishReason(
    StartupProfilingFinishReason finish_reason) {
  RecordFirstWebContentsFinishReason(finish_reason);
}

void FirstWebContentsProfiler::RecordNavigationFinished(
    base::TimeTicks navigation_start) {
  startup_metric_utils::GetBrowser().RecordFirstWebContentsMainNavigationStart(
      navigation_start);
  startup_metric_utils::GetBrowser()
      .RecordFirstWebContentsMainNavigationFinished(base::TimeTicks::Now());
}

void FirstWebContentsProfiler::RecordFirstNonEmptyPaint() {
  startup_metric_utils::GetBrowser().RecordFirstWebContentsNonEmptyPaint(
      base::TimeTicks::Now(),
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetLastInitTime());
}

bool FirstWebContentsProfiler::WasStartupInterrupted() {
  return startup_metric_utils::GetBrowser().WasMainWindowStartupInterrupted();
}

}  // namespace

void BeginFirstWebContentsProfiling() {
  content::WebContents* visible_contents = nullptr;
  const BrowserList* browser_list = BrowserList::GetInstance();
  for (Browser* browser : browser_list->OrderedByActivation()) {
    visible_contents =
        FirstWebContentsProfilerBase::GetVisibleContents(browser);
    if (visible_contents)
      break;
  }

  if (!visible_contents) {
    RecordFirstWebContentsFinishReason(
        StartupProfilingFinishReason::kAbandonNoInitiallyVisibleContent);
    return;
  }

  // FirstWebContentsProfiler owns itself and is also bound to
  // |visible_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfiler(visible_contents);
}

}  // namespace metrics
