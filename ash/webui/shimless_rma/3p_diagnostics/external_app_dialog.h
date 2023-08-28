// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_3P_DIAGNOSTICS_EXTERNAL_APP_DIALOG_H_
#define ASH_WEBUI_SHIMLESS_RMA_3P_DIAGNOSTICS_EXTERNAL_APP_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace views {
class WebDialogView;
}  // namespace views

namespace ash::shimless_rma {

// Provides a dialog to show external apps on shimless rma screen. It creates a
// WebDialogView, and disables some API (e.g. opening tabs, opening file
// chooser) to prevent them being used in shimless rma screen.
class ExternalAppDialog : public ui::WebDialogDelegate,
                          public content::WebContentsObserver {
 public:
  using ConsoleLogCallback =
      base::RepeatingCallback<void(logging::LogSeverity log_level,
                                   const std::u16string& message,
                                   int32_t line_no,
                                   const std::u16string& source_id)>;
  // Params for a dialog.
  struct InitParams {
    InitParams();
    InitParams(const InitParams&) = delete;
    InitParams& operator=(const InitParams&) = delete;
    ~InitParams();

    // The BrowserContext for the dialog.
    raw_ptr<content::BrowserContext> context;
    // The url of the dialog content.
    GURL content_url;
    // Callback for handling the console log from the app.
    ConsoleLogCallback on_console_log;
  };

  // Shows the dialog. Shouldn't be called if last dialog opened by this
  // function is still open.
  static void Show(const InitParams& params);

  // Returns the WebContents of the dialog. Could be `nullptr` if WebContents is
  // not ready.
  static content::WebContents* GetWebContents();

 protected:
  explicit ExternalAppDialog(const InitParams& params);
  ExternalAppDialog(const ExternalAppDialog&) = delete;
  ExternalAppDialog& operator=(const ExternalAppDialog&) = delete;

  ~ExternalAppDialog() override;

 private:
  // ui::WebDialogDelegate overrides:
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetDialogSize(gfx::Size* size) const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  std::string GetDialogArgs() const override;
  void OnLoadingStateChanged(content::WebContents* source) override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCenterDialogTitleText() const override;
  bool ShouldShowCloseButton() const override;

  // content::WebContentsObserver overrides:
  void OnDidAddMessageToConsole(
      content::RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const absl::optional<std::u16string>& untrusted_stack_trace) override;

  // The url of the content.
  GURL content_url_;
  // views::WebDialogView that owns this delegate.
  raw_ptr<views::WebDialogView> web_dialog_view_;
  // Callback for handling the console log from the app.
  ConsoleLogCallback on_console_log_;
};

}  // namespace ash::shimless_rma

#endif  // ASH_WEBUI_SHIMLESS_RMA_3P_DIAGNOSTICS_EXTERNAL_APP_DIALOG_H_
