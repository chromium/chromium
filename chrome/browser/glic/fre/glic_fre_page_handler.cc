// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_page_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicFrePageHandler::GlicFrePageHandler(
    bool is_unified_fre,
    content::WebContents* webui_contents,
    mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver)
    : content::WebContentsObserver(webui_contents),
      is_unified_fre_(is_unified_fre),
      webui_contents_(webui_contents),
      receiver_(this, std::move(receiver)) {
  auto timestamps =
      GetGlicService()->fre_controller().RegisterPageHandler(this);
  open_start_time_ = timestamps.open_start_time;
  framework_start_time_ = timestamps.framework_start_time;
}

GlicFrePageHandler::~GlicFrePageHandler() {
  LogDismissalMetrics();
  if (auto* service = GetGlicService()) {
    service->fre_controller().UnregisterPageHandler(this);
  }
  WebUiStateChanged(mojom::FreWebUiState::kUninitialized);
}

void GlicFrePageHandler::OnVisibilityChanged(content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE) {
    // The modal FRE should behave like it used to: keep one timer running
    // until explicitly closed, even if the user temporarily switches tabs.
    if (!is_unified_fre_) {
      return;
    }
    // Reset session timers for a warmed start. Only do this if we were
    // previously dismissed/hidden to avoid overwriting the initial cold start
    // time.
    if (close_reason_ == CloseReason::kDismissed) {
      open_start_time_ = base::TimeTicks::Now();
      // If we are already ready, restart interaction timer immediately.
      if (webui_state_ == mojom::FreWebUiState::kReady) {
        interaction_timer_.emplace();
      }
      // Reset state to active so we can log the next dismissal.
      close_reason_ = CloseReason::kActive;
    }
  } else if (visibility == content::Visibility::HIDDEN) {
    // Treat hiding as a dismissal.
    LogDismissalMetrics();
  }
}

void GlicFrePageHandler::LogDismissalMetrics() {
  // Only log if we are currently active.
  if (close_reason_ != CloseReason::kActive) {
    return;
  }
  // Skip TotalTime if the start timestamp was missing or already consumed to
  // avoid invalid durations.
  if (!open_start_time_.is_null()) {
    base::UmaHistogramMediumTimes(is_unified_fre_
                                      ? "Glic.UnifiedFre.TotalTime.Dismissed"
                                      : "Glic.Fre.TotalTime.Dismissed",
                                  base::TimeTicks::Now() - open_start_time_);
  }
  if (interaction_timer_) {
    base::UmaHistogramTimes(is_unified_fre_
                                ? "Glic.UnifiedFre.InteractionTime.Dismissed"
                                : "Glic.Fre.InteractionTime.Dismissed",
                            interaction_timer_->Elapsed());
    interaction_timer_.reset();
  }
  // Mark as dismissed so we don't log again until it becomes visible.
  close_reason_ = CloseReason::kDismissed;
}

content::BrowserContext* GlicFrePageHandler::browser_context() const {
  return webui_contents_->GetBrowserContext();
}

GlicKeyedService* GlicFrePageHandler::GetGlicService() {
  return GlicKeyedServiceFactory::GetGlicKeyedService(browser_context());
}

void GlicFrePageHandler::AcceptFre() {
  // Log metrics for this specific instance. Skip TotalTime if the start
  // timestamp was missing or already consumed to avoid invalid durations.
  if (!open_start_time_.is_null()) {
    base::UmaHistogramMediumTimes(is_unified_fre_
                                      ? "Glic.UnifiedFre.TotalTime.Accepted"
                                      : "Glic.Fre.TotalTime.Accepted",
                                  base::TimeTicks::Now() - open_start_time_);
  }
  if (interaction_timer_) {
    base::UmaHistogramTimes(is_unified_fre_
                                ? "Glic.UnifiedFre.InteractionTime.Accepted"
                                : "Glic.Fre.InteractionTime.Accepted",
                            interaction_timer_->Elapsed());
  }
  close_reason_ = CloseReason::kAccepted;
  GetGlicService()->metrics()->OnFreAccepted();
  GetGlicService()->fre_controller().AcceptFre(this);
}

void GlicFrePageHandler::RejectFre() {
  // Log metrics for this specific instance. Skip TotalTime if the start
  // timestamp was missing or already consumed to avoid invalid durations.
  if (!open_start_time_.is_null()) {
    base::UmaHistogramMediumTimes(is_unified_fre_
                                      ? "Glic.UnifiedFre.TotalTime.NoThanks"
                                      : "Glic.Fre.TotalTime.NoThanks",
                                  base::TimeTicks::Now() - open_start_time_);
  }
  if (interaction_timer_) {
    base::UmaHistogramTimes(is_unified_fre_
                                ? "Glic.UnifiedFre.InteractionTime.NoThanks"
                                : "Glic.Fre.InteractionTime.NoThanks",
                            interaction_timer_->Elapsed());
  }
  close_reason_ = CloseReason::kRejected;
  GetGlicService()->fre_controller().RejectFre();
}

void GlicFrePageHandler::OnAcceptedByOtherInstance() {
  // Skip TotalTime if the start timestamp was missing or already consumed to
  // avoid invalid durations.
  if (close_reason_ != CloseReason::kActive || open_start_time_.is_null()) {
    return;
  }
  // Another instance accepted/rejected. Log that this instance lost the race.
  base::UmaHistogramMediumTimes(
      "Glic.UnifiedFre.TotalTime.AcceptedByOtherInstance",
      base::TimeTicks::Now() - open_start_time_);
  close_reason_ = CloseReason::kAcceptedByOtherInstance;
}

void GlicFrePageHandler::DismissFre(mojom::FreWebUiState panel_state) {
  GetGlicService()->fre_controller().DismissFre(panel_state);
}

void GlicFrePageHandler::FreReloaded() {
  base::RecordAction(
      base::UserMetricsAction("Glic.Fre.ErrorPanelTryAgainClicked"));
}

void GlicFrePageHandler::PrepareForClient(
    base::OnceCallback<void(bool)> callback) {
  GetGlicService()->fre_controller().PrepareForClient(std::move(callback));
}

void GlicFrePageHandler::ValidateAndOpenLinkInNewTab(const GURL& url) {
  if (url.DomainIs("google.com")) {
    GetGlicService()->CreateTab(url, /*open_in_background=*/true, std::nullopt,
                                base::DoNothing());
    GetGlicService()->fre_controller().OnLinkClicked(url);
  }
}

void GlicFrePageHandler::WebUiStateChanged(mojom::FreWebUiState new_state) {
  if (webui_state_ == new_state) {
    return;
  }
  webui_state_ = new_state;
  if (webui_state_ == mojom::FreWebUiState::kReady) {
    base::RecordAction(base::UserMetricsAction("Glic.Fre.LoadSuccess"));
    if (web_client_load_timer_) {
      base::UmaHistogramMediumTimes(is_unified_fre_
                                        ? "Glic.UnifiedFre.WebClientLoadTime"
                                        : "Glic.Fre.WebClientLoadTime",
                                    web_client_load_timer_->Elapsed());
      // Reset so we don't record again on a re-render without a full reload.
      web_client_load_timer_.reset();
    }
    // Only if we have a valid start time and haven't hit an error state before.
    if (!open_start_time_.is_null()) {
      base::UmaHistogramMediumTimes("Glic.FrePresentationTime",
                                    base::TimeTicks::Now() - open_start_time_);
    }
    interaction_timer_.emplace();
  }
  // It is possible for the FRE to open directly in an error state. In this
  // case, we should not record the FRE load time metric if the content is
  // loaded at a later point.
  if (webui_state_ == mojom::FreWebUiState::kError ||
      webui_state_ == mojom::FreWebUiState::kOffline) {
    // Invalidate the start time so PresentationTime won't be recorded later.
    open_start_time_ = base::TimeTicks();
  }
  GetGlicService()->fre_controller().WebUiStateChanged(webui_state_);
}

void GlicFrePageHandler::ExceededTimeoutError() {
  GetGlicService()->fre_controller().ExceededTimeoutError();
}

void GlicFrePageHandler::LogWebUiLoadComplete() {
  if (!framework_start_time_.is_null()) {
    base::UmaHistogramMediumTimes(
        is_unified_fre_ ? "Glic.UnifiedFre.WebUiFrameworkLoadTime"
                        : "Glic.Fre.WebUiFrameworkLoadTime",
        base::TimeTicks::Now() - framework_start_time_);
    framework_start_time_ = base::TimeTicks();
  }
  web_client_load_timer_.emplace();
}

}  // namespace glic
