// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "printing/buildflags/buildflags.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
struct GlobalRenderFrameHostId;
}

namespace printing {

// Manages the print commands for a WebContents.
class PrintViewManager : public PrintViewManagerBase,
                         public content::WebContentsUserData<PrintViewManager> {
 public:
  PrintViewManager(const PrintViewManager&) = delete;
  PrintViewManager& operator=(const PrintViewManager&) = delete;

  ~PrintViewManager() override;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<mojom::PrintManagerHost> receiver,
      content::RenderFrameHost* rfh);

  // Same as PrintNow(), but for the case where a user prints with the system
  // dialog from print preview.
  // `dialog_shown_callback` is called when the print dialog is shown.
  bool PrintForSystemDialogNow(base::OnceClosure dialog_shown_callback);

  // Same as PrintNow(), but for the case where a user press "ctrl+shift+p" to
  // show the native system dialog. This can happen from both initiator and
  // preview dialog.
  bool BasicPrint(content::RenderFrameHost* rfh);

  // Initiate print preview of the current document and specify whether a
  // selection or the entire frame is being printed.
  bool PrintPreviewNow(content::RenderFrameHost* rfh, bool has_selection);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Initiate print preview of the current document and provide the renderer
  // a printing::mojom::PrintRenderer to perform the actual rendering of
  // the print document.
  bool PrintPreviewWithPrintRenderer(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer);
#endif

  // Initiate print preview for the node under the context menu.
  void PrintPreviewForNodeUnderContextMenu(content::RenderFrameHost* rfh);

  // Notify PrintViewManager that print preview is about to finish. Unblock the
  // renderer in the case of scripted print preview if needed.
  void PrintPreviewAlmostDone();

  // Notify PrintViewManager that print preview has finished. Unblock the
  // renderer in the case of scripted print preview if needed.
  void PrintPreviewDone();

  // mojom::PrintManagerHost:
  void DidShowPrintDialog() override;
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override;
  void ShowScriptedPrintPreview(bool source_is_modifiable) override;
  void RequestPrintPreview(mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;

  // content::WebContentsObserver implementation.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  content::RenderFrameHost* print_preview_rfh() { return print_preview_rfh_; }

  // Sets the target object for BindPrintManagerHost() for tests.
  static void SetReceiverImplForTesting(PrintManager* impl);

 protected:
  explicit PrintViewManager(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<PrintViewManager>;

  enum PrintPreviewState {
    NOT_PREVIEWING,
    USER_INITIATED_PREVIEW,
    SCRIPTED_PREVIEW,
  };

  // Helper method for PrintPreviewNow() and PrintPreviewWithRenderer().
  // Initiate print preview of the current document by first notifying the
  // renderer. Since this happens asynchronously, the print preview dialog
  // creation will not be completed on the return of this function. Returns
  // false if print preview is impossible at the moment.
  bool PrintPreview(
      content::RenderFrameHost* rfh,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
#endif
      bool has_selection);

  // Notify PrintViewManager that print preview is starting in the renderer for
  // a particular WebNode.
  void PrintPreviewForWebNode(content::RenderFrameHost* rfh);

  // Helper method for ShowScriptedPrintPreview(), called from
  // RejectPrintPreviewRequestIfRestricted(). Based on value of
  // `should_proceed`, continues to show the print preview or cancels it.
  void OnScriptedPrintPreviewCallback(bool source_is_modifiable,
                                      content::GlobalRenderFrameHostId rfh_id,
                                      bool should_proceed);

  // Helper method for RequestPrintPreview(), called from
  // RejectPrintPreviewRequestIfRestricted(). Based on value of
  // `should_proceed`, continues to show the print preview or cancels it.
  void OnRequestPrintPreviewCallback(mojom::RequestPrintPreviewParamsPtr params,
                                     content::GlobalRenderFrameHostId rfh_id,
                                     bool should_proceed);

  void MaybeUnblockScriptedPreviewRPH();

  // Helper used to set `print_preview_rfh_` to `rfh`. Performs sanity checks
  // before doing so.
  void SetPrintPreviewRenderFrameHost(content::RenderFrameHost* rfh);

  // Checks whether printing is currently restricted and aborts print preview if
  // needed. Since this check is performed asynchronously, invokes `callback`
  // with an indicator whether to proceed or not.
  // Virtual to allow tests to override.
  virtual void RejectPrintPreviewRequestIfRestricted(
      content::GlobalRenderFrameHostId rfh_id,
      base::OnceCallback<void(bool should_proceed)> callback);

  // Helper method for RejectPrintPreviewRequestIfRestricted(). Handles any
  // tasks that need to be done when the request is rejected due to
  // restrictions.
  void OnPrintPreviewRequestRejected(content::GlobalRenderFrameHostId rfh_id);

  // Virtual method that tests can override, in order to avoid actually
  // displaying a system print dialog.
  virtual void PrintForSystemDialogImpl();

  // Virtual method to be overridden in tests, in order to be notified whether
  // the print preview is shown or not due to policies or user actions.
  virtual void PrintPreviewRejectedForTesting();
  // Virtual method to be overridden in tests, in order to be notified when the
  // print preview is not prevented by policies or user actions.
  virtual void PrintPreviewAllowedForTesting();

  base::OnceClosure on_print_dialog_shown_callback_;

  // Current state of print preview for this view.
  PrintPreviewState print_preview_state_ = NOT_PREVIEWING;

  // The current RFH that is print previewing. It should be a nullptr when
  // `print_preview_state_` is NOT_PREVIEWING.
  raw_ptr<content::RenderFrameHost> print_preview_rfh_ = nullptr;

  // Keeps track of the pending callback during scripted print preview.
  raw_ptr<content::RenderProcessHost> scripted_print_preview_rph_ = nullptr;

  // True if `scripted_print_preview_rph_` needs to be unblocked.
  bool scripted_print_preview_rph_set_blocked_ = false;

  // Indicates whether we're switching from print preview to system dialog. This
  // flag is true between PrintForSystemDialogNow() and PrintPreviewDone().
  bool is_switching_to_system_dialog_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Keep this last so that all weak pointers will be invalidated at the
  // beginning of destruction. Note that PrintViewManagerBase has its own
  // base::WeakPtrFactory as well, but PrintViewManager should use this one.
  base::WeakPtrFactory<PrintViewManager> weak_factory_{this};
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
