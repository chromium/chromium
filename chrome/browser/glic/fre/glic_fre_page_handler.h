// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_FRE_GLIC_FRE_PAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_FRE_GLIC_FRE_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"

class GURL;
namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace glic {
class GlicKeyedService;

// Handles the Mojo requests coming from the Glic FRE WebUI.
class GlicFrePageHandler : public glic::mojom::FrePageHandler,
                           public content::WebContentsObserver {
 public:
  GlicFrePageHandler(
      bool is_unified_fre,
      content::WebContents* webui_contents,
      mojo::PendingReceiver<glic::mojom::FrePageHandler> receiver);

  GlicFrePageHandler(const GlicFrePageHandler&) = delete;
  GlicFrePageHandler& operator=(const GlicFrePageHandler&) = delete;

  ~GlicFrePageHandler() override;

  // glic::mojom::FrePageHandler implementation.
  void AcceptFre() override;
  void RejectFre() override;
  void DismissFre(mojom::FreWebUiState panel_state) override;
  void FreReloaded() override;
  void PrepareForClient(base::OnceCallback<void(bool)> callback) override;
  void ValidateAndOpenLinkInNewTab(const GURL& url) override;
  void WebUiStateChanged(mojom::FreWebUiState new_state) override;
  void ExceededTimeoutError() override;
  void LogWebUiLoadComplete() override;

  // Called by the controller when another FRE instance has accepted the FRE.
  // This instance should log a specific metric indicating it lost the race.
  void OnAcceptedByOtherInstance();

 private:
  content::BrowserContext* browser_context() const;
  GlicKeyedService* GetGlicService();

  // content::WebContentsObserver overrides:
  void OnVisibilityChanged(content::Visibility visibility) override;

  void LogDismissalMetrics();

  const bool is_unified_fre_;

  // Safe because `this` is owned by GlicUI` ,which is a `MojoWebUIController`
  // and won't outlive its `WebContents`.
  const raw_ptr<content::WebContents> webui_contents_;
  mojo::Receiver<glic::mojom::FrePageHandler> receiver_;

  // Tracks the total time since the FRE completed loading and has entered the
  // Ready state.
  std::optional<base::ElapsedTimer> interaction_timer_;

  // Tracks elapsed time between the start of the web client loading and the
  // moment it's fully loaded.
  std::optional<base::ElapsedTimer> web_client_load_timer_;

  // Used to track the total time this specific FRE instance has been open.
  // It is also used to measure the time between the request for the FRE to show
  // to the time it is fully loaded and showing (presentation time). This value
  // is determined immediately upon construction.
  base::TimeTicks open_start_time_;

  // Used to track the time between the start of the WebUI framework loading and
  // the moment it's fully loaded. This ends right before the web client begins
  // loading.
  base::TimeTicks framework_start_time_;

  mojom::FreWebUiState webui_state_ = mojom::FreWebUiState::kUninitialized;

  enum class CloseReason {
    kActive,                   // Still open.
    kAccepted,                 // User clicked "Allow" in this instance.
    kRejected,                 // User clicked "No thanks" in this instance.
    kDismissed,                // Closed by other means (e.g. tab close).
    kAcceptedByOtherInstance,  // Another multi-instance FRE accepted.
  };
  CloseReason close_reason_ = CloseReason::kActive;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_FRE_GLIC_FRE_PAGE_HANDLER_H_
