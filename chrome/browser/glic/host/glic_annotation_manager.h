// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ANNOTATION_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ANNOTATION_MANAGER_H_

#include <ostream>

#include "base/callback_list.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "content/public/browser/weak_document_ptr.h"
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

  // Removes any existing annotations.
  void RemoveAnnotation(mojom::ScrollToErrorReason reason);

 private:
  // Represents the processing of a single `ScrollTo` call.
  // Note: The task is currently kept alive after the scroll is triggered and
  // `scroll_to_callback_` is run to keep the text highlight alive in the
  // renderer (highlighting is removed when `annotation_agent_` is reset). The
  // highlight is currently persisted until either the page with the highlight
  // is navigated from or ScrollTo() is called again.
  class AnnotationTask : public blink::mojom::AnnotationAgentHost,
                         content::WebContentsObserver,
                         GlicWindowController::StateObserver {
   public:
    AnnotationTask(GlicAnnotationManager* manager,
                   mojo::Remote<blink::mojom::AnnotationAgent> annotation_agent,
                   mojo::PendingReceiver<blink::mojom::AnnotationAgentHost>
                       annotation_agent_host,
                   mojom::WebClientHandler::ScrollToCallback callback,
                   content::Page& page);
    ~AnnotationTask() override;

    // Returns true if the task is still running, false if it is complete. The
    // ScrollTo callback hasn't been run yet if this is true.
    bool IsRunning() const;

    // Fails the task with `reason` if it's still running, otherwise drops the
    // active annotation.
    void FailTaskOrDropAnnotation(mojom::ScrollToErrorReason reason);

   private:
    enum class State {
      // The initial state of a task. The renderer is searching for the
      // selected content and `scroll_to_callback_` hasn't run.
      kRunning,

      // The task failed (before or during the search), and
      // `scroll_to_callback_` was resolved with a failure. All mojo connections
      // and subscriptions have been reset.
      kFailed,

      // The renderer has found the selected content (DidFinishAttachment was
      // called), and content is being scrolled to and highlighted.
      // `scroll_to_callback` has been resolved successfully (without an error).
      kActive,

      // The selection was dropped (after being active initially).
      // `scroll_to_callback` has been resolved successfully (without an
      // error). All mojo connections and subscriptions have been reset.
      kInactive,
    };

    static std::string ToString(State state);
    friend std::ostream& operator<<(std::ostream& o, State state) {
      o << ToString(state);
      return o;
    }
    void SetState(State new_state);

    void RemoteDisconnected();
    void DropAnnotation();
    void ResetConnections();

    // Runs the callback with `error_reason` and invalidates `this`. Should only
    // be called when `IsRunning()` is true.
    void FailTask(mojom::ScrollToErrorReason error_reason);

    // blink::mojom::AnnotationAgentHost overrides.
    void DidFinishAttachment(
        const gfx::Rect& document_relative_rect,
        blink::mojom::AttachmentResult attachment_result) override;

    // content::WebContentsObserver overrides.
    void PrimaryPageChanged(content::Page& page) override;

    // `GlicWindowController::StateObserver`:
    void PanelStateChanged(const mojom::PanelState& panel_state,
                           Browser* attached_browser) override;

    // GlicFocusedTabManager::FocusedTabChangedCallback
    void OnFocusedTabChanged(FocusedTabData focused_tab_data);

    // `pref_change_registrar_` callback.
    void OnTabContextPermissionChanged(const std::string& pref_name);

    // Uniquely owns `this`.
    base::raw_ref<GlicAnnotationManager> annotation_manager_;

    // Used for bi-directional communication with `page_`'s main document's
    // AnnotationAgent.
    mojo::Remote<blink::mojom::AnnotationAgent> annotation_agent_;
    mojo::Receiver<blink::mojom::AnnotationAgentHost>
        annotation_agent_host_receiver_;

    // Callback for ScrollTo() that's run when the task completes or fails.
    mojom::WebClientHandler::ScrollToCallback scroll_to_callback_;

    // Page this task is running in.
    base::WeakPtr<content::Page> page_;

    // Subscription to listen to focused tab changes/primary page navigations
    // while the task is running. Cleared after the task completes/fails.
    base::CallbackListSubscription tab_change_subscription_;

    // Used to subscribe to tab context permission changes.
    PrefChangeRegistrar pref_change_registrar_;

    // Current state of the task, see documentation for `State`.
    State state_ = State::kRunning;

    // Used to record the match duration of `ScrollTo()`.
    const base::TimeTicks start_time_;
  };

  // See documentation for `annotation_agent_container_` below.
  struct AnnotationAgentContainer {
    AnnotationAgentContainer();
    ~AnnotationAgentContainer();

    content::WeakDocumentPtr document;
    mojo::Remote<blink::mojom::AnnotationAgentContainer> remote;
  };

  // `GlicKeyedService` instance associated with the `GlicWebClientHandler`
  // that owns `this`. Will outlive `this`.
  const raw_ptr<GlicKeyedService> service_;

  // Set when this class binds to a remote AnnotationAgentContainer when
  // ScrollTo is called. It also tracks the specific document it connected to,
  // which allows us to reuse the connection if we receive another ScrollTo
  // request for the same document.
  std::optional<AnnotationAgentContainer> annotation_agent_container_;

  // Keeps track of the currently running ScrollTo call. See documentation for
  // `AnnotationTask`.
  std::unique_ptr<AnnotationTask> annotation_task_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ANNOTATION_MANAGER_H_
