// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_client_responsiveness_monitor.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicWebClientResponsivenessMonitor::GlicWebClientResponsivenessMonitor(
    Delegate* delegate,
    content::RenderFrameHost* guest_main_frame,
    StateChangedCallback state_changed_callback)
    : delegate_(delegate),
      guest_main_frame_(guest_main_frame
                            ? guest_main_frame->GetWeakDocumentPtr()
                            : content::WeakDocumentPtr()),
      state_changed_callback_(std::move(state_changed_callback)) {
  CHECK(state_changed_callback_ && delegate_);
}

GlicWebClientResponsivenessMonitor::~GlicWebClientResponsivenessMonitor() {
  Stop();
}

void GlicWebClientResponsivenessMonitor::Start() {
  if (!base::FeatureList::IsEnabled(features::kGlicClientResponsivenessCheck)) {
    return;
  }

  int interval_ms = features::kGlicClientResponsivenessCheckIntervalMs.Get();
  if (interval_ms <= 0) {
    interval_ms = 5000;
  }

  ping_timer_.Start(
      FROM_HERE, base::Milliseconds(interval_ms),
      base::BindRepeating(&GlicWebClientResponsivenessMonitor::OnTimerTick,
                          base::Unretained(this)));
}

void GlicWebClientResponsivenessMonitor::Stop() {
  ping_timer_.Stop();
  timeout_timer_.Stop();
  error_timer_.Stop();
}

void GlicWebClientResponsivenessMonitor::OnTimerTick() {
  if (current_state_ == mojom::WebClientState::kError) {
    return;
  }

  if (!timeout_timer_.IsRunning()) {
    int timeout_ms = features::kGlicClientResponsivenessCheckTimeoutMs.Get();
    if (timeout_ms <= 0) {
      timeout_ms = 15000;
    }
    timeout_timer_.Start(
        FROM_HERE, base::Milliseconds(timeout_ms),
        base::BindOnce(
            &GlicWebClientResponsivenessMonitor::OnCheckResponsiveTimeout,
            base::Unretained(this)));
  }

  delegate_->CheckResponsive(base::BindOnce(
      &GlicWebClientResponsivenessMonitor::OnCheckResponsiveResponse,
      weak_ptr_factory_.GetWeakPtr()));
}

void GlicWebClientResponsivenessMonitor::OnCheckResponsiveResponse() {
  if (current_state_ == mojom::WebClientState::kError) {
    return;
  }

  timeout_timer_.Stop();
  error_timer_.Stop();
  if (current_state_ != mojom::WebClientState::kResponsive) {
    current_state_ = mojom::WebClientState::kResponsive;
    state_changed_callback_.Run(current_state_);
  }
}

void GlicWebClientResponsivenessMonitor::OnCheckResponsiveTimeout() {
  if (features::kGlicClientResponsivenessCheckIgnoreWhenDebuggerAttached
          .Get()) {
    content::RenderFrameHost* rfh =
        guest_main_frame_.AsRenderFrameHostIfValid();
    if (rfh) {
      content::WebContents* web_contents =
          content::WebContents::FromRenderFrameHost(rfh);
      if (web_contents &&
          content::DevToolsAgentHost::IsDebuggerAttached(web_contents)) {
        if (!has_shown_debugger_attached_warning_) {
          LOG(WARNING)
              << "GlicWebClientResponsivenessMonitor: ignoring unresponsive "
                 "client because DevTools/debugger is attached";
          has_shown_debugger_attached_warning_ = true;
        }
        return;
      }
    }
  }

  if (current_state_ == mojom::WebClientState::kResponsive) {
    current_state_ = mojom::WebClientState::kUnresponsive;
    state_changed_callback_.Run(current_state_);

    int error_timeout_ms = features::kGlicClientUnresponsiveUiMaxTimeMs.Get();
    if (error_timeout_ms <= 0) {
      error_timeout_ms = 5000;
    }
    error_timer_.Start(
        FROM_HERE, base::Milliseconds(error_timeout_ms),
        base::BindOnce(
            &GlicWebClientResponsivenessMonitor::OnUnresponsiveErrorTimeout,
            base::Unretained(this)));
  }
}

void GlicWebClientResponsivenessMonitor::OnUnresponsiveErrorTimeout() {
  if (current_state_ == mojom::WebClientState::kUnresponsive) {
    current_state_ = mojom::WebClientState::kError;
    ping_timer_.Stop();
    timeout_timer_.Stop();
    state_changed_callback_.Run(current_state_);
  }
}

}  // namespace glic
