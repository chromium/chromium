// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_

#include "base/macros.h"
#include "chrome/browser/printing/print_view_manager_base.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "printing/buildflags/buildflags.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
}

namespace printing {

// Manages the print commands for a WebContents.
class PrintViewManager : public PrintViewManagerBase,
                         public content::WebContentsUserData<PrintViewManager> {
 public:
  ~PrintViewManager() override;

  // Same as PrintNow(), but for the case where a user prints with the system
  // dialog from print preview.
  // |dialog_shown_callback| is called when the print dialog is shown.
  bool PrintForSystemDialogNow(base::OnceClosure dialog_shown_callback);

  // Same as PrintNow(), but for the case where a user press "ctrl+shift+p" to
  // show the native system dialog. This can happen from both initiator and
  // preview dialog.
  bool BasicPrint(content::RenderFrameHost* rfh);

  // Initiate print preview of the current document and specify whether a
  // selection or the entire frame is being printed.
  bool PrintPreviewNow(content::RenderFrameHost* rfh, bool has_selection);

  // Initiate print preview of the current document and provide the renderer
  // a printing::mojom::PrintRenderer to perform the actual rendering of
  // the print document.
  bool PrintPreviewWithPrintRenderer(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer);

  // Notify PrintViewManager that print preview is starting in the renderer for
  // a particular WebNode.
  void PrintPreviewForWebNode(content::RenderFrameHost* rfh);

  // Notify PrintViewManager that print preview is about to finish. Unblock the
  // renderer in the case of scripted print preview if needed.
  void PrintPreviewAlmostDone();

  // Notify PrintViewManager that print preview has finished. Unblock the
  // renderer in the case of scripted print preview if needed.
  void PrintPreviewDone();

  // Checks whether printing is currently restricted and aborts print preview if
  // needed.
  bool RejectPrintPreviewRequestIfRestricted(content::RenderFrameHost* rfh);

  // mojom::PrintManagerHost:
  void DidShowPrintDialog() override;

  // content::WebContentsObserver implementation.
  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override;

  content::RenderFrameHost* print_preview_rfh() { return print_preview_rfh_; }

 protected:
  explicit PrintViewManager(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<PrintViewManager>;

  enum PrintPreviewState {
    NOT_PREVIEWING,
    USER_INITIATED_PREVIEW,
    SCRIPTED_PREVIEW,
  };

  struct FrameDispatchHelper;

  // Helper method for PrintPreviewNow() and PrintPreviewWithRenderer().
  // Initiate print preview of the current document by first notifying the
  // renderer. Since this happens asynchronously, the print preview dialog
  // creation will not be completed on the return of this function. Returns
  // false if print preview is impossible at the moment.
  bool PrintPreview(
      content::RenderFrameHost* rfh,
      mojo::PendingAssociatedRemote<mojom::PrintRenderer> print_renderer,
      bool has_selection);

  // IPC Message handlers.
  void OnSetupScriptedPrintPreview(content::RenderFrameHost* rfh,
                                   IPC::Message* reply_msg);
  void OnShowScriptedPrintPreview(content::RenderFrameHost* rfh,
                                  bool source_is_modifiable);
  void OnScriptedPrintPreviewReply(IPC::Message* reply_msg);

  void MaybeUnblockScriptedPreviewRPH();

  // Checks whether printing is restricted due to Data Leak Protection rules.
  bool IsPrintingRestricted() const;

  base::OnceClosure on_print_dialog_shown_callback_;

  // Current state of print preview for this view.
  PrintPreviewState print_preview_state_ = NOT_PREVIEWING;

  // The current RFH that is print previewing. It should be a nullptr when
  // |print_preview_state_| is NOT_PREVIEWING.
  content::RenderFrameHost* print_preview_rfh_ = nullptr;

  // Keeps track of the pending callback during scripted print preview.
  content::RenderProcessHost* scripted_print_preview_rph_ = nullptr;

  // True if |scripted_print_preview_rph_| needs to be unblocked.
  bool scripted_print_preview_rph_set_blocked_ = false;

  // Indicates whether we're switching from print preview to system dialog. This
  // flag is true between PrintForSystemDialogNow() and PrintPreviewDone().
  bool is_switching_to_system_dialog_ = false;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(PrintViewManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_VIEW_MANAGER_H_
