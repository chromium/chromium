// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_RESPONSIVENESS_MONITOR_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_RESPONSIVENESS_MONITOR_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "content/public/browser/weak_document_ptr.h"

namespace content {
class RenderFrameHost;
}

namespace glic {

// Monitors the health and responsiveness of the Glic web client.
//
// Periodically pings the web client and reports state transitions between
// responsive, unresponsive, and error states. This allows the browser to
// detect when the client UI is frozen or unresponsive and update the UX
// accordingly. The error state is terminal and halts further monitoring.
//
// To support development and debugging, unresponsiveness detection is
// automatically bypassed when DevTools is attached to the web client frame
// and the GlicClientResponsivenessCheckIgnoreWhenDebuggerAttached feature flag
// is enabled
class GlicWebClientResponsivenessMonitor {
 public:
  // Delegate used to send responsiveness checks to the web client.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Initiates a responsiveness check and invokes `callback` when a response
    // is received.
    virtual void CheckResponsive(base::OnceClosure callback) = 0;
  };

  using StateChangedCallback =
      base::RepeatingCallback<void(mojom::WebClientState state)>;

  // `delegate` must outlive this monitor.
  GlicWebClientResponsivenessMonitor(
      Delegate* delegate,
      content::RenderFrameHost* guest_main_frame,
      StateChangedCallback state_changed_callback);
  ~GlicWebClientResponsivenessMonitor();

  // Starts periodic responsiveness monitoring.
  void Start();

  // Stops responsiveness monitoring.
  void Stop();
  bool is_responsive() const {
    return current_state_ == mojom::WebClientState::kResponsive;
  }
  mojom::WebClientState current_state() const { return current_state_; }

 private:
  void OnTimerTick();
  void OnCheckResponsiveResponse();
  void OnCheckResponsiveTimeout();
  void OnUnresponsiveErrorTimeout();

  raw_ptr<Delegate> delegate_;
  content::WeakDocumentPtr guest_main_frame_;
  StateChangedCallback state_changed_callback_;
  base::RepeatingTimer ping_timer_;
  base::OneShotTimer timeout_timer_;
  base::OneShotTimer error_timer_;
  mojom::WebClientState current_state_ = mojom::WebClientState::kResponsive;
  bool has_shown_debugger_attached_warning_ = false;

  base::WeakPtrFactory<GlicWebClientResponsivenessMonitor> weak_ptr_factory_{
      this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_RESPONSIVENESS_MONITOR_H_
