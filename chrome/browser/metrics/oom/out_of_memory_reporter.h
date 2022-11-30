// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_OOM_OUT_OF_MEMORY_REPORTER_H_
#define CHROME_BROWSER_METRICS_OOM_OUT_OF_MEMORY_REPORTER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/child_process_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#endif

class OutOfMemoryReporterTest;

// This class listens for OOM notifications from WebContentsObserver and
// crash_reporter::CrashMetricsReporter::Observer methods. It forwards
// foreground OOM notifications to observers.
class OutOfMemoryReporter
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OutOfMemoryReporter>
#if BUILDFLAG(IS_ANDROID)
    ,
      public crash_reporter::CrashMetricsReporter::Observer
#endif
{
 public:
  class Observer {
   public:
    virtual void OnForegroundOOMDetected(const GURL& url,
                                         ukm::SourceId source_id) = 0;
  };

  OutOfMemoryReporter(const OutOfMemoryReporter&) = delete;
  OutOfMemoryReporter& operator=(const OutOfMemoryReporter&) = delete;

  ~OutOfMemoryReporter() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend class content::WebContentsUserData<OutOfMemoryReporter>;
  friend class OutOfMemoryReporterTest;

  OutOfMemoryReporter(content::WebContents* web_contents);
  void OnForegroundOOMDetected(const GURL& url, ukm::SourceId source_id);

  // Used by tests to deterministically control time.
  void SetTickClockForTest(std::unique_ptr<const base::TickClock> tick_clock);

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus termination_status) override;

#if BUILDFLAG(IS_ANDROID)
  // crash_reporter::CrashMetricsReporter::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override;
#endif  // BUILDFLAG(IS_ANDROID)

  base::ObserverList<Observer>::Unchecked observers_;

  absl::optional<ukm::SourceId> last_committed_source_id_;
  base::TimeTicks last_navigation_timestamp_;
  std::unique_ptr<const base::TickClock> tick_clock_;
  int crashed_render_process_id_ = content::ChildProcessHost::kInvalidUniqueID;

#if BUILDFLAG(IS_ANDROID)
  base::ScopedObservation<crash_reporter::CrashMetricsReporter,
                          crash_reporter::CrashMetricsReporter::Observer>
      scoped_observation_{this};
#endif
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_METRICS_OOM_OUT_OF_MEMORY_REPORTER_H_
