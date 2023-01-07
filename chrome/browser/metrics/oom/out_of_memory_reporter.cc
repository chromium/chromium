// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/oom/out_of_memory_reporter.h"

#include <utility>

#include "base/check.h"
#include "base/observer_list.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

OutOfMemoryReporter::~OutOfMemoryReporter() {}

void OutOfMemoryReporter::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OutOfMemoryReporter::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

OutOfMemoryReporter::OutOfMemoryReporter(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OutOfMemoryReporter>(*web_contents),
      tick_clock_(std::make_unique<base::DefaultTickClock>()) {
#if BUILDFLAG(IS_ANDROID)
  // This adds N async observers for N WebContents, which isn't great but
  // probably won't be a big problem on Android, where many multiple tabs are
  // rarer.
  auto* crash_manager = crash_reporter::CrashMetricsReporter::GetInstance();
  DCHECK(crash_manager);
  scoped_observation_.Observe(crash_manager);
#endif
}

void OutOfMemoryReporter::OnForegroundOOMDetected(const GURL& url,
                                                  ukm::SourceId source_id) {
  DCHECK(!last_navigation_timestamp_.is_null());
  base::TimeDelta time_since_last_navigation =
      tick_clock_->NowTicks() - last_navigation_timestamp_;
  ukm::builders::Tab_RendererOOM(source_id)
      .SetTimeSinceLastNavigation(time_since_last_navigation.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
  for (auto& observer : observers_) {
    observer.OnForegroundOOMDetected(url, source_id);
  }
}

void OutOfMemoryReporter::SetTickClockForTest(
    std::unique_ptr<const base::TickClock> tick_clock) {
  DCHECK(tick_clock_);
  tick_clock_ = std::move(tick_clock);
}

void OutOfMemoryReporter::PrimaryPageChanged(content::Page& page) {
  last_committed_source_id_.reset();
  last_navigation_timestamp_ = tick_clock_->NowTicks();
  crashed_render_process_id_ = content::ChildProcessHost::kInvalidUniqueID;
  if (page.GetMainDocument().IsErrorDocument())
    return;
  last_committed_source_id_ = page.GetMainDocument().GetPageUkmSourceId();
}

void OutOfMemoryReporter::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // Don't record OOM metrics (especially not UKM) for unactivated portals
  // since the user didn't explicitly navigate to it.
  if (web_contents()->IsPortal())
    return;
  if (!last_committed_source_id_.has_value())
    return;
  if (web_contents()->GetVisibility() != content::Visibility::VISIBLE)
    return;

  // RenderProcessGone is only called for when the current RenderFrameHost of
  // the primary main frame exits, so it is ok to call GetPrimaryMainFrame here.
  crashed_render_process_id_ =
      web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID();

// On Android, we care about OOM protected crashes, which are obtained via
// crash dump analysis. Otherwise we can use the termination status to
// determine OOM.
#if !BUILDFLAG(IS_ANDROID)
  if (status == base::TERMINATION_STATUS_OOM
#if BUILDFLAG(IS_CHROMEOS)
      || status == base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM
#endif
  ) {
    OnForegroundOOMDetected(web_contents()->GetLastCommittedURL(),
                            *last_committed_source_id_);
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if BUILDFLAG(IS_ANDROID)
// This should always be called *after* the associated RenderProcessGone. This
// is because the crash dump is processed asynchronously on the IO thread in
// response to RenderProcessHost::ProcessDied, while RenderProcessGone is called
// synchronously from the call to ProcessDied.
void OutOfMemoryReporter::OnCrashDumpProcessed(
    int rph_id,
    const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
        reported_counts) {
  if (!last_committed_source_id_.has_value())
    return;
  // Make sure the crash happened in the correct RPH.
  if (rph_id != crashed_render_process_id_)
    return;

  if (reported_counts.count(
          crash_reporter::CrashMetricsReporter::ProcessedCrashCounts::
              kRendererForegroundVisibleOom)) {
    OnForegroundOOMDetected(web_contents()->GetLastCommittedURL(),
                            *last_committed_source_id_);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)

WEB_CONTENTS_USER_DATA_KEY_IMPL(OutOfMemoryReporter);
