// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_

#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/printing/print_preview/print_preview_dialog_controller_cros.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_ui_wrapper.h"
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_base.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "stdint.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace chromeos {

// Implements PrintViewManagerCrosBase and is the main implementor for printing
// commands. Facilitates calls from browser to printing services. One instance
// exists per print preview dialog.
class PrintViewManagerCros
    : public PrintViewManagerCrosBase,
      public ash::PrintPreviewDialogControllerCros::DialogControllerObserver,
      public content::WebContentsUserData<PrintViewManagerCros> {
 public:
  explicit PrintViewManagerCros(content::WebContents* web_contents);
  PrintViewManagerCros(const PrintViewManagerCros&) = delete;
  PrintViewManagerCros& operator=(const PrintViewManagerCros&) = delete;

  ~PrintViewManagerCros() override;

  static void BindPrintManagerHost(
      mojo::PendingAssociatedReceiver<::printing::mojom::PrintManagerHost>
          receiver,
      content::RenderFrameHost* rfh);

  // mojom::PrintManagerHost:
  void DidShowPrintDialog() override;
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override;
  void ShowScriptedPrintPreview(bool source_is_modifiable) override;
  // Called when a webcontent requests to open a new instance of print preview.
  void RequestPrintPreview(
      ::printing::mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;

  // content::WebContentsObserver::
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  bool PrintPreviewNow(content::RenderFrameHost* rfh, bool has_selection);
  // Called when a webcontent is done (canceled or completed) and therefore its
  // print preview must be closed.
  void PrintPreviewDone();

  // Inform the PrintRenderFrame that the dialog has been removed and clears out
  // the render frame host associated with this instance.
  void HandlePrintPreviewRemoved();

  // ash::PrintPreviewDialogControllerCros::DialogControllerObserver:
  // Handle when the print preview dialog is closed by navigation. For
  // example, closing dialog via Exit navigation button.
  void OnDialogClosed(const base::UnguessableToken& token) override;

  content::RenderFrameHost* render_frame_host_for_testing() {
    return render_frame_host_;
  }

 private:
  friend class content::WebContentsUserData<PrintViewManagerCros>;

  // The current RFH that is print previewing.
  raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;

  // Own the instance of the UI wrapper, this holds wrapper for a print preview
  // UI endpoint for renderer communication.
  std::unique_ptr<PrintPreviewUiWrapper> ui_wrapper_;

  // TODO(crbug.com/365741912, crbug.com/365902693): Since Ash and Chrome share
  // the same process, we can now use the WebContents directly instead of this
  // token.
  //
  // Unique ID of the webcontent tied to this instance. This token is created
  // by this class and is passed to clients interested in identifying the
  // webcontents.
  base::UnguessableToken token_;

  base::ScopedObservation<
      ash::PrintPreviewDialogControllerCros,
      ash::PrintPreviewDialogControllerCros::DialogControllerObserver>
      dialog_controller_observation_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_
