// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_dialog_controller.h"

#include <utility>

#include "chrome/browser/media/router/media_router_metrics.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/route_request_result.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom.h"

namespace media_router {

class MediaRouterDialogController::InitiatorWebContentsObserver
    : public content::WebContentsObserver {
 public:
  InitiatorWebContentsObserver(content::WebContents* web_contents,
                               MediaRouterDialogController* dialog_controller)
      : content::WebContentsObserver(web_contents),
        dialog_controller_(dialog_controller) {
    DCHECK(dialog_controller_);
  }

 private:
  void WebContentsDestroyed() override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  void RenderProcessGone(base::TerminationStatus status) override {
    // NOTE: |this| is deleted after CloseMediaRouterDialog() returns.
    dialog_controller_->CloseMediaRouterDialog();
  }

  MediaRouterDialogController* const dialog_controller_;
};

MediaRouterDialogController::MediaRouterDialogController(
    content::WebContents* initiator)
    : initiator_(initiator) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initiator_);
}

MediaRouterDialogController::~MediaRouterDialogController() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

bool MediaRouterDialogController::ShowMediaRouterDialogForPresentation(
    std::unique_ptr<StartPresentationContext> context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsShowingMediaRouterDialog()) {
    context->InvokeErrorCallback(blink::mojom::PresentationError(
        blink::mojom::PresentationErrorType::UNKNOWN,
        "Unable to create dialog: dialog already shown"));
    return false;
  }

  start_presentation_context_ = std::move(context);
  MediaRouterMetrics::RecordMediaRouterDialogOrigin(
      MediaRouterDialogOpenOrigin::PAGE);
  FocusOnMediaRouterDialog(true);
  return true;
}

bool MediaRouterDialogController::ShowMediaRouterDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  bool dialog_needs_creation = !IsShowingMediaRouterDialog();
  FocusOnMediaRouterDialog(dialog_needs_creation);
  return dialog_needs_creation;
}

void MediaRouterDialogController::HideMediaRouterDialog() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CloseMediaRouterDialog();
  Reset();
}

void MediaRouterDialogController::FocusOnMediaRouterDialog(
    bool dialog_needs_creation) {
  // Show the WebContents requesting a dialog.
  // TODO(takumif): In the case of Views dialog, if the dialog is already shown,
  // activating the WebContents makes the dialog lose focus and disappear. The
  // dialog needs to be created again in that case.
  initiator_->GetDelegate()->ActivateContents(initiator_);
  if (dialog_needs_creation) {
    initiator_observer_ =
        std::make_unique<InitiatorWebContentsObserver>(initiator_, this);
    CreateMediaRouterDialog();
  }
}

void MediaRouterDialogController::Reset() {
  initiator_observer_.reset();
  start_presentation_context_.reset();
}

}  // namespace media_router
