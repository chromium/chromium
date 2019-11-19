// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_

#include <memory>

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/android/oom_intervention/near_oom_monitor.h"
#include "chrome/browser/ui/interventions/intervention_delegate.h"
#include "components/crash/content/browser/crash_metrics_reporter_android.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/oom_intervention/oom_intervention.mojom.h"

namespace content {
class WebContents;
}

class OomInterventionDecider;

// A tab helper for near-OOM intervention.
class OomInterventionTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<OomInterventionTabHelper>,
      public crash_reporter::CrashMetricsReporter::Observer,
      public blink::mojom::OomInterventionHost,
      public InterventionDelegate {
 public:
  static bool IsEnabled();

  ~OomInterventionTabHelper() override;

  // blink::mojom::OomInterventionHost:
  void OnHighMemoryUsage() override;

  // InterventionDelegate:
  void AcceptIntervention() override;
  void DeclineIntervention() override;
  void DeclineInterventionWithReload() override;
  void DeclineInterventionSticky() override;

 private:
  explicit OomInterventionTabHelper(content::WebContents* web_contents);

  friend class content::WebContentsUserData<OomInterventionTabHelper>;

  // content::WebContentsObserver:
  void WebContentsDestroyed() override;
  void RenderProcessGone(base::TerminationStatus status) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void OnVisibilityChanged(content::Visibility visibility) override;
  void DocumentOnLoadCompletedInMainFrame() override;

  // CrashDumpManager::Observer:
  void OnCrashDumpProcessed(
      int rph_id,
      const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
          reported_counts) override;

  // Starts observing near-OOM situation if it's not started.
  void StartMonitoringIfNeeded();
  // Stops observing near-OOM situation.
  void StopMonitoring();
  // Starts detecting near-OOM situation in renderer.
  void StartDetectionInRenderer();

  // Called when NearOomMonitor detects near-OOM situation.
  void OnNearOomDetected();

  // Called when we stop monitoring high memory usage in the foreground
  // renderer.
  void OnDetectionWindowElapsedWithoutHighMemoryUsage();

  void ResetInterventionState();

  void ResetInterfaces();

  bool navigation_started_ = false;
  bool load_finished_ = false;
  base::Optional<base::TimeTicks> near_oom_detected_time_;
  std::unique_ptr<NearOomMonitor::Subscription> subscription_;
  base::OneShotTimer renderer_detection_timer_;

  // Not owned. This will be nullptr in incognito mode.
  OomInterventionDecider* decider_;

  mojo::Remote<blink::mojom::OomIntervention> intervention_;

  enum class InterventionState {
    // Intervention isn't triggered yet.
    NOT_TRIGGERED,
    // Intervention is triggered but the user doesn't respond yet.
    UI_SHOWN,
    // Intervention is triggered and the user declined it.
    DECLINED,
    // Intervention is triggered and the user accepted it.
    ACCEPTED,
  };

  InterventionState intervention_state_ = InterventionState::NOT_TRIGGERED;

  mojo::Receiver<blink::mojom::OomInterventionHost> receiver_{this};

  // The shared memory region that stores metrics written by the renderer
  // process. The memory is updated frequently and the browser should touch the
  // memory only after renderer process is dead.
  base::UnsafeSharedMemoryRegion shared_metrics_buffer_;
  base::WritableSharedMemoryMapping metrics_mapping_;

  base::TimeTicks last_navigation_timestamp_;
  base::TimeTicks start_monitor_timestamp_;

  ScopedObserver<crash_reporter::CrashMetricsReporter,
                 crash_reporter::CrashMetricsReporter::Observer>
      scoped_observer_;

  base::WeakPtrFactory<OomInterventionTabHelper> weak_ptr_factory_{this};
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_ANDROID_OOM_INTERVENTION_OOM_INTERVENTION_TAB_HELPER_H_
