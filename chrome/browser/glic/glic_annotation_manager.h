// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_ANNOTATION_MANAGER_H_
#define CHROME_BROWSER_GLIC_GLIC_ANNOTATION_MANAGER_H_

#include "base/callback_list.h"
#include "chrome/browser/glic/glic.mojom-shared.h"
#include "chrome/browser/glic/glic_tab_data.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/annotation/annotation.mojom.h"

namespace glic {

class GlicKeyedService;

// Manages annotation (scroll-to and highlight) requests for Glic. Owned by
// and 1:1 with `GlicWebClientHandler`.
class GlicAnnotationManager {
 public:
  explicit GlicAnnotationManager(GlicKeyedService* service);
  ~GlicAnnotationManager();

  // Scrolls to and highlights content in its owner's (GlicKeyedService)
  // currently focused tab. |callback| is run after the content is found in
  // the renderer process, and a scroll is triggered, or if a failure occurs.
  // (See ScrollToErrorReason in glic.mojom for a list of possible failure
  // reasons.)
  // Note: This currently only supports scrolling to and highlighting based on
  // a single selector. If this is called a second time before finishing
  // the first request, the first request is cancelled.
  // TODO(crbug.com/397664100): Support scrolling without highlighting.
  // TODO(crbug.com/395859365): Support PDFs.
  void ScrollTo(mojom::ScrollToParamsPtr params,
                mojom::WebClientHandler::ScrollToCallback callback);

 private:
  // Represents the processing of a single `ScrollTo` call. It is currently
  // destroyed when a failure occurs or when a new request is started.
  // Note: The task is currently kept alive after the scroll is triggered and
  // `scroll_to_callback_` is run to keep the text highlight alive in the
  // renderer (highlighting is removed when `annotation_agent_` is reset).
  class AnnotationTask : public blink::mojom::AnnotationAgentHost {
   public:
    AnnotationTask(mojo::Remote<blink::mojom::AnnotationAgent> annotation_agent,
                   mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
                       annotation_agent_host,
                   mojom::WebClientHandler::ScrollToCallback callback);
    ~AnnotationTask() override;

    // Runs the callback with `error_reason` (if the callback hasn't already
    // been run). Resets mojo connections.
    void MaybeFailTask(mojom::ScrollToErrorReason error_reason);

    // blink::mojom::AnnotationAgentHost overrides.
    void DidFinishAttachment(const gfx::Rect& document_relative_rect) override;

   private:
    mojo::Remote<blink::mojom::AnnotationAgent> annotation_agent_;
    mojo::Receiver<blink::mojom::AnnotationAgentHost>
        annotation_agent_host_receiver_;
    mojom::WebClientHandler::ScrollToCallback scroll_to_callback_;
  };

  void MaybeFailAndResetTask(glic::mojom::ScrollToErrorReason);
  void OnFocusedTabChanged(FocusedTabData focused_tab_data);

  // `GlicKeyedService` instance associated with the `GlicWebClientHandler`
  // that owns `this`. Will outlive `this`.
  const raw_ptr<GlicKeyedService> service_;

  // When bound, this is bound to `service_`'s currently focused tab's primary
  // main frame.
  mojo::Remote<blink::mojom::AnnotationAgentContainer>
      annotation_agent_container_;

  // Subscription to listen to focused tab changes/primary page navigations.
  base::CallbackListSubscription tab_change_subscription_;

  // Currently focused tab's (retrieved from
  // `GlicKeyedService::GetFocusedTabData`) primary page.
  base::WeakPtr<content::Page> focused_primary_page_;

  // Keeps track of the currently running ScrollTo call. See documentation for
  // `AnnotationTask`.
  std::unique_ptr<AnnotationTask> annotation_task_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_ANNOTATION_MANAGER_H_
