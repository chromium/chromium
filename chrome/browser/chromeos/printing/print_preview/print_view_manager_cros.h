// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_
#define CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_

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

// Implements PrintViewManagerCrosBase and is the main implementor for printing
// commands. Facilitates calls from browser to printing services. One instance
// exists per print preview dialog.
class PrintViewManagerCros
    : public PrintViewManagerCrosBase,
      public content::WebContentsUserData<PrintViewManagerCros> {
 public:
  PrintViewManagerCros(const PrintViewManagerCros&) = delete;
  PrintViewManagerCros& operator=(const PrintViewManagerCros&) = delete;

  ~PrintViewManagerCros() override = default;

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

  content::RenderFrameHost* render_frame_host_for_testing() {
    return render_frame_host_;
  }

 protected:
  explicit PrintViewManagerCros(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<PrintViewManagerCros>;

  // The current RFH that is print previewing.
  raw_ptr<content::RenderFrameHost> render_frame_host_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PRINTING_PRINT_PREVIEW_PRINT_VIEW_MANAGER_CROS_H_
