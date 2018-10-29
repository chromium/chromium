// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_config.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "chrome/browser/ui/android/infobars/near_oom_reduction_infobar.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"

namespace {

constexpr base::TimeDelta kRendererHighMemoryUsageDetectionWindow =
    base::TimeDelta::FromSeconds(60);

content::WebContents* g_last_visible_web_contents = nullptr;

bool IsLastVisibleWebContents(content::WebContents* web_contents) {
  return web_contents == g_last_visible_web_contents;
}

void SetLastVisibleWebContents(content::WebContents* web_contents) {
  g_last_visible_web_contents = web_contents;
}

// These enums are associated with UMA. Values must be kept in sync with
// enums.xml and must not be renumbered/reused.
enum class NearOomDetectionEndReason {
  OOM_PROTECTED_CRASH = 0,
  RENDERER_GONE = 1,
  NAVIGATION = 2,
  COUNT,
};

void RecordNearOomDetectionEndReason(NearOomDetectionEndReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "Memory.Experimental.OomIntervention.NearOomDetectionEndReason", reason,
      NearOomDetectionEndReason::COUNT);
}

void RecordInterventionUserDecision(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN("Memory.Experimental.OomIntervention.UserDecision",
                        accepted);
}

void RecordInterventionStateOnCrash(bool accepted) {
  UMA_HISTOGRAM_BOOLEAN(
      "Memory.Experimental.OomIntervention.InterventionStateOnCrash", accepted);
}

}  // namespace

// static
bool OomInterventionTabHelper::IsEnabled() {
  return OomInterventionConfig::GetInstance()->is_intervention_enabled();
}

OomInterventionTabHelper::OomInterventionTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      decider_(OomInterventionDecider::GetForBrowserContext(
          web_contents->GetBrowserContext())),
      binding_(this),
      scoped_observer_(this),
      weak_ptr_factory_(this) {
  scoped_observer_.Add(crash_reporter::CrashMetricsReporter::GetInstance());
}

OomInterventionTabHelper::~OomInterventionTabHelper() = default;

void OomInterventionTabHelper::OnHighMemoryUsage() {
  auto* config = OomInterventionConfig::GetInstance();
  if (config->is_renderer_pause_enabled() ||
      config->is_navigate_ads_enabled()) {
    NearOomReductionInfoBar::Show(web_contents(), this);
    intervention_state_ = InterventionState::UI_SHOWN;
    if (!last_navigation_timestamp_.is_null()) {
      base::TimeDelta time_since_last_navigation =
          base::TimeTicks::Now() - last_navigation_timestamp_;
      UMA_HISTOGRAM_COUNTS_1M(
          "Memory.Experimental.OomIntervention."
          "RendererTimeSinceLastNavigationAtIntervention",
          time_since_last_navigation.InSeconds());
    }
  }
  near_oom_detected_time_ = base::TimeTicks::Now();
  renderer_detection_timer_.AbandonAndStop();
}

void OomInterventionTabHelper::AcceptIntervention() {
  RecordInterventionUserDecision(true);
  intervention_state_ = InterventionState::ACCEPTED;
}

void OomInterventionTabHelper::DeclineIntervention() {
  RecordInterventionUserDecision(false);
  ResetInterfaces();
  intervention_state_ = InterventionState::DECLINED;

  if (decider_) {
    DCHECK(!web_contents()->GetBrowserContext()->IsOffTheRecord());
    const std::string& host = web_contents()->GetVisibleURL().host();
    decider_->OnInterventionDeclined(host);
  }
}

void OomInterventionTabHelper::DeclineInterventionWithReload() {
  web_contents()->GetController().Reload(content::ReloadType::NORMAL, true);
  DeclineIntervention();
}

void OomInterventionTabHelper::DeclineInterventionSticky() {
  NOTREACHED();
}

void OomInterventionTabHelper::WebContentsDestroyed() {
  StopMonitoring();
}

void OomInterventionTabHelper::RenderProcessGone(
    base::TerminationStatus status) {
  ResetInterfaces();

  // Skip background process termination.
  if (!IsLastVisibleWebContents(web_contents())) {
    ResetInterventionState();
    return;
  }

  // OOM crash is handled in OnForegroundOOMDetected().
  if (status == base::TERMINATION_STATUS_OOM_PROTECTED)
    return;

  if (near_oom_detected_time_) {
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - near_oom_detected_time_.value();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Memory.Experimental.OomIntervention."
        "RendererGoneAfterDetectionTime",
        elapsed_time);
    ResetInterventionState();
  } else {
    RecordNearOomDetectionEndReason(NearOomDetectionEndReason::RENDERER_GONE);
  }
}

void OomInterventionTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Filter out sub-frame's navigation or if the navigation happens without
  // changing document.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  last_navigation_timestamp_ = base::TimeTicks::Now();

  // Filter out the first navigation.
  if (!navigation_started_) {
    navigation_started_ = true;
    return;
  }

  ResetInterfaces();

  // Filter out background navigation.
  if (!IsLastVisibleWebContents(navigation_handle->GetWebContents())) {
    ResetInterventionState();
    return;
  }

  if (near_oom_detected_time_) {
    // near-OOM was detected.
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - near_oom_detected_time_.value();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Memory.Experimental.OomIntervention."
        "NavigationAfterDetectionTime",
        elapsed_time);
    ResetInterventionState();
  } else {
    // Monitoring but near-OOM hasn't been detected.
    RecordNearOomDetectionEndReason(NearOomDetectionEndReason::NAVIGATION);
  }
}

void OomInterventionTabHelper::DocumentAvailableInMainFrame() {
  if (IsLastVisibleWebContents(web_contents()))
    StartMonitoringIfNeeded();
}

void OomInterventionTabHelper::OnVisibilityChanged(
    content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    StartMonitoringIfNeeded();
    SetLastVisibleWebContents(web_contents());
  } else {
    StopMonitoring();
  }
}

void OomInterventionTabHelper::OnCrashDumpProcessed(
    int rph_id,
    const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
        reported_counts) {
  if (rph_id != web_contents()->GetMainFrame()->GetProcess()->GetID())
    return;
  if (!reported_counts.count(
          crash_reporter::CrashMetricsReporter::ProcessedCrashCounts::
              kRendererForegroundVisibleOom)) {
    return;
  }

  DCHECK(IsLastVisibleWebContents(web_contents()));
  if (near_oom_detected_time_) {
    base::TimeDelta elapsed_time =
        base::TimeTicks::Now() - near_oom_detected_time_.value();
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "Memory.Experimental.OomIntervention."
        "OomProtectedCrashAfterDetectionTime",
        elapsed_time);

    if (intervention_state_ != InterventionState::NOT_TRIGGERED) {
      // Consider UI_SHOWN as ACCEPTED because we already triggered the
      // intervention and the user didn't decline.
      bool accepted = intervention_state_ != InterventionState::DECLINED;
      RecordInterventionStateOnCrash(accepted);
    }
    ResetInterventionState();
  } else {
    RecordNearOomDetectionEndReason(
        NearOomDetectionEndReason::OOM_PROTECTED_CRASH);
  }

  base::TimeDelta time_since_last_navigation;
  if (!last_navigation_timestamp_.is_null()) {
    time_since_last_navigation =
        base::TimeTicks::Now() - last_navigation_timestamp_;
  }
  UMA_HISTOGRAM_COUNTS_1M(
      "Memory.Experimental.OomIntervention."
      "RendererTimeSinceLastNavigationAtOOM",
      time_since_last_navigation.InSeconds());

  if (decider_) {
    DCHECK(!web_contents()->GetBrowserContext()->IsOffTheRecord());
    const std::string& host = web_contents()->GetVisibleURL().host();
    decider_->OnOomDetected(host);
  }
}

void OomInterventionTabHelper::StartMonitoringIfNeeded() {
  if (subscription_)
    return;

  if (intervention_)
    return;

  if (near_oom_detected_time_)
    return;

  auto* config = OomInterventionConfig::GetInstance();
  if (config->should_detect_in_renderer()) {
    if (binding_.is_bound())
      return;
    StartDetectionInRenderer();
  } else if (config->is_swap_monitor_enabled()) {
    subscription_ = NearOomMonitor::GetInstance()->RegisterCallback(
        base::BindRepeating(&OomInterventionTabHelper::OnNearOomDetected,
                            base::Unretained(this)));
  }
}

void OomInterventionTabHelper::StopMonitoring() {
  if (OomInterventionConfig::GetInstance()->should_detect_in_renderer()) {
    ResetInterfaces();
  } else {
    subscription_.reset();
  }
}

void OomInterventionTabHelper::StartDetectionInRenderer() {
  auto* config = OomInterventionConfig::GetInstance();
  bool renderer_pause_enabled = config->is_renderer_pause_enabled();
  bool navigate_ads_enabled = config->is_navigate_ads_enabled();

  if ((renderer_pause_enabled || navigate_ads_enabled) && decider_) {
    DCHECK(!web_contents()->GetBrowserContext()->IsOffTheRecord());
    const std::string& host = web_contents()->GetVisibleURL().host();
    if (!decider_->CanTriggerIntervention(host)) {
      renderer_pause_enabled = false;
      navigate_ads_enabled = false;
    }
  }

  if (!renderer_pause_enabled && !navigate_ads_enabled)
    return;
  content::RenderFrameHost* main_frame = web_contents()->GetMainFrame();
  DCHECK(main_frame);
  content::RenderProcessHost* render_process_host = main_frame->GetProcess();
  DCHECK(render_process_host);
  content::BindInterface(render_process_host,
                         mojo::MakeRequest(&intervention_));
  DCHECK(!binding_.is_bound());
  blink::mojom::OomInterventionHostPtr host;
  binding_.Bind(mojo::MakeRequest(&host));
  blink::mojom::DetectionArgsPtr detection_args =
      config->GetRendererOomDetectionArgs();
  intervention_->StartDetection(std::move(host), std::move(detection_args),
                                renderer_pause_enabled, navigate_ads_enabled);
}

void OomInterventionTabHelper::OnNearOomDetected() {
  DCHECK(!OomInterventionConfig::GetInstance()->should_detect_in_renderer());
  DCHECK_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  DCHECK(!near_oom_detected_time_);
  subscription_.reset();

  StartDetectionInRenderer();
  DCHECK(!renderer_detection_timer_.IsRunning());
  renderer_detection_timer_.Start(
      FROM_HERE, kRendererHighMemoryUsageDetectionWindow,
      base::BindRepeating(&OomInterventionTabHelper::
                              OnDetectionWindowElapsedWithoutHighMemoryUsage,
                          weak_ptr_factory_.GetWeakPtr()));
}

void OomInterventionTabHelper::
    OnDetectionWindowElapsedWithoutHighMemoryUsage() {
  ResetInterventionState();
  ResetInterfaces();
  StartMonitoringIfNeeded();
}

void OomInterventionTabHelper::ResetInterventionState() {
  near_oom_detected_time_.reset();
  intervention_state_ = InterventionState::NOT_TRIGGERED;
  renderer_detection_timer_.AbandonAndStop();
}

void OomInterventionTabHelper::ResetInterfaces() {
  intervention_.reset();
  if (binding_.is_bound())
    binding_.Close();
}
