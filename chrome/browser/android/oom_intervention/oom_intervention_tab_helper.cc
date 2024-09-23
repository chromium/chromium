// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_tab_helper.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_config.h"
#include "chrome/browser/android/oom_intervention/oom_intervention_decider.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/messages/android/messages_feature.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/oom_intervention/oom_intervention_types.h"

namespace {

constexpr base::TimeDelta kRendererHighMemoryUsageDetectionWindow =
    base::Seconds(60);

content::WebContents* g_last_visible_web_contents = nullptr;

bool IsLastVisibleWebContents(content::WebContents* web_contents) {
  return web_contents == g_last_visible_web_contents;
}

void SetLastVisibleWebContents(content::WebContents* web_contents) {
  g_last_visible_web_contents = web_contents;
}

}  // namespace

// static
bool OomInterventionTabHelper::IsEnabled() {
  return OomInterventionConfig::GetInstance()->is_intervention_enabled();
}

OomInterventionTabHelper::OomInterventionTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<OomInterventionTabHelper>(*web_contents),
      decider_(OomInterventionDecider::GetForBrowserContext(
          web_contents->GetBrowserContext())) {
  scoped_observation_.Observe(
      crash_reporter::CrashMetricsReporter::GetInstance());
}

OomInterventionTabHelper::~OomInterventionTabHelper() = default;

void OomInterventionTabHelper::OnHighMemoryUsage() {
  auto* config = OomInterventionConfig::GetInstance();
  if (config->is_renderer_pause_enabled() ||
      config->is_navigate_ads_enabled() ||
      config->is_purge_v8_memory_enabled()) {
    near_oom_reduction_message_delegate_.ShowMessage(web_contents(), this);
    intervention_state_ = InterventionState::UI_SHOWN;
  }

  near_oom_detected_time_ = base::TimeTicks::Now();
  renderer_detection_timer_.AbandonAndStop();
}

void OomInterventionTabHelper::AcceptIntervention() {
  intervention_state_ = InterventionState::ACCEPTED;
}

void OomInterventionTabHelper::DeclineIntervention() {
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
  NOTREACHED_IN_MIGRATION();
}

void OomInterventionTabHelper::WebContentsDestroyed() {
  StopMonitoring();
}

void OomInterventionTabHelper::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  ResetInterfaces();

  // Skip background process termination.
  if (!IsLastVisibleWebContents(web_contents())) {
    ResetInterventionState();
    return;
  }

  if (near_oom_detected_time_) {
    ResetInterventionState();
  }
}

void OomInterventionTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  // Filter out sub-frame's navigation, non-primary page's navigation, or if the
  // navigation happens without changing document.
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

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
    ResetInterventionState();
  }
}

void OomInterventionTabHelper::PrimaryPageChanged(content::Page& page) {
  if (!page.GetMainDocument().IsDocumentOnLoadCompletedInMainFrame())
    return;
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

void OomInterventionTabHelper::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (IsLastVisibleWebContents(web_contents()))
    StartMonitoringIfNeeded();
}

void OomInterventionTabHelper::OnCrashDumpProcessed(
    int rph_id,
    const crash_reporter::CrashMetricsReporter::ReportedCrashTypeSet&
        reported_counts) {
  if (rph_id !=
      web_contents()->GetPrimaryPage().GetMainDocument().GetProcess()->GetID())
    return;
  if (!reported_counts.count(
          crash_reporter::CrashMetricsReporter::ProcessedCrashCounts::
              kRendererForegroundVisibleOom)) {
    return;
  }

  DCHECK(IsLastVisibleWebContents(web_contents()));
  if (near_oom_detected_time_) {
    ResetInterventionState();
  }

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

  if (!web_contents()->IsDocumentOnLoadCompletedInPrimaryMainFrame())
    return;

  auto* config = OomInterventionConfig::GetInstance();
  if (config->should_detect_in_renderer()) {
    if (receiver_.is_bound())
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
    subscription_ = {};
  }
}

void OomInterventionTabHelper::StartDetectionInRenderer() {
  auto* config = OomInterventionConfig::GetInstance();
  bool renderer_pause_enabled = config->is_renderer_pause_enabled();
  bool navigate_ads_enabled = config->is_navigate_ads_enabled();
  bool purge_v8_memory_enabled = config->is_purge_v8_memory_enabled();

  if ((renderer_pause_enabled || navigate_ads_enabled ||
       purge_v8_memory_enabled) &&
      decider_) {
    DCHECK(!web_contents()->GetBrowserContext()->IsOffTheRecord());
    const std::string& host = web_contents()->GetVisibleURL().host();
    if (!decider_->CanTriggerIntervention(host)) {
      return;
    }
  }

  content::RenderFrameHost& main_frame =
      web_contents()->GetPrimaryPage().GetMainDocument();

  // Connections to the renderer will not be recreated when coming out of the
  // cache so prevent us from getting in there in the first place.
  content::BackForwardCache::DisableForRenderFrameHost(
      &main_frame,
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kOomInterventionTabHelper));

  content::RenderProcessHost* render_process_host = main_frame.GetProcess();
  DCHECK(render_process_host);
  render_process_host->BindReceiver(intervention_.BindNewPipeAndPassReceiver());
  DCHECK(!receiver_.is_bound());
  blink::mojom::DetectionArgsPtr detection_args =
      config->GetRendererOomDetectionArgs();
  intervention_->StartDetection(
      receiver_.BindNewPipeAndPassRemote(), std::move(detection_args),
      renderer_pause_enabled, navigate_ads_enabled, purge_v8_memory_enabled);
}

void OomInterventionTabHelper::OnNearOomDetected() {
  DCHECK(!OomInterventionConfig::GetInstance()->should_detect_in_renderer());
  DCHECK_EQ(web_contents()->GetVisibility(), content::Visibility::VISIBLE);
  DCHECK(!near_oom_detected_time_);
  subscription_ = {};

  StartDetectionInRenderer();
  DCHECK(!renderer_detection_timer_.IsRunning());
  renderer_detection_timer_.Start(
      FROM_HERE, kRendererHighMemoryUsageDetectionWindow,
      base::BindOnce(&OomInterventionTabHelper::
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
  receiver_.reset();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(OomInterventionTabHelper);
