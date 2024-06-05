// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_

#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/print_preview/print_preview_ui_wrapper.h"
#include "chrome/browser/chromeos/printing/print_preview/print_view_manager_cros_base.h"
#include "components/printing/common/print.mojom-forward.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "stdint.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace chromeos {

namespace printing {
class PrintPreviewWebContentsManagerBrowserTest;
}  // namespace printing

// Implements PrintViewManagerCrosBase and is the main implementor for printing
// commands. Facilitates calls from browser to printing services. One instance
// exists per print preview dialog.
class PrintViewManagerCros
    : public PrintViewManagerCrosBase,
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
  void RequestPrintPreview(
      ::printing::mojom::RequestPrintPreviewParamsPtr params) override;
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override;

  // content::WebContentsObserver::
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;

  bool PrintPreviewNow(content::RenderFrameHost* rfh, bool has_selection);
  void PrintPreviewDone();

  // Start the print preview generation.
  void HandleGeneratePrintPreview(const base::Value::Dict& settings);
  // Inform the PrintRenderFrame that the dialog has been removed and clears out
  // the render frame host associated with this instance.
  void HandlePrintPreviewRemoved();

  content::RenderFrameHost* render_frame_host_for_testing() {
    return render_frame_host_;
  }

 private:
  friend class PrintViewManagerCrosTest;
  friend class chromeos::printing::PrintPreviewWebContentsManagerBrowserTest;
  friend class content::WebContentsUserData<PrintViewManagerCros>;

  // Some tests will not bind to the renderer, this allows tests to directly
  // inject a RenderFrameHost.
  void set_render_frame_host_for_testing(content::RenderFrameHost* rfh) {
    render_frame_host_ = rfh;
  }

  // The current RFH that is print previewing.
  raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;

  // Own the instance of the UI wrapper, this holds wrapper for a print preview
  // UI endpoint for renderer communication.
  std::unique_ptr<PrintPreviewUiWrapper> ui_wrapper_;

  // Unique ID of the webcontent tied to this instance. This token is created
  // by this class and is passed to clients interested in identifying the
  // webcontents.
  base::UnguessableToken token_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_
