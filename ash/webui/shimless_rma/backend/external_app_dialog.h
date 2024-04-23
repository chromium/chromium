// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_BACKEND_EXTERNAL_APP_DIALOG_H_
#define ASH_WEBUI_SHIMLESS_RMA_BACKEND_EXTERNAL_APP_DIALOG_H_

#include <string>

#include "ash/webui/shimless_rma/backend/shimless_rma_delegate.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace views {
class WebDialogView;
class Widget;
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
    // App name.
    std::string app_name;
    // The url of the dialog content.
    GURL content_url;
    // Callback for handling the console log from the app.
    ConsoleLogCallback on_console_log;
    // The shimless RMA delegate for accessing //chrome functions.
    base::WeakPtr<ShimlessRmaDelegate> shimless_rma_delegate;
  };

  // Shows the dialog. Shouldn't be called if last dialog opened by this
  // function is still open.
  static void Show(const InitParams& params);

  // Returns the WebContents of the dialog. Could be `nullptr` if WebContents is
  // not ready.
  static content::WebContents* GetWebContents();

  // Sets a callback to mock `Show` in test.
  static void SetMockShowForTesting(
      base::RepeatingCallback<void(const InitParams& params)> callback);

  // Closes the open dialog in test. Does nothing if there is no open dialog.
  static void CloseForTesting();

 protected:
  explicit ExternalAppDialog(const InitParams& params);
  ExternalAppDialog(const ExternalAppDialog&) = delete;
  ExternalAppDialog& operator=(const ExternalAppDialog&) = delete;

  ~ExternalAppDialog() override;

 private:
  // ui::WebDialogDelegate overrides:
  void GetDialogSize(gfx::Size* size) const override;
  void OnLoadingStateChanged(content::WebContents* source) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;

  // content::WebContentsObserver overrides:
  void OnDidAddMessageToConsole(
      content::RenderFrameHost* source_frame,
      blink::mojom::ConsoleMessageLevel log_level,
      const std::u16string& message,
      int32_t line_no,
      const std::u16string& source_id,
      const std::optional<std::u16string>& untrusted_stack_trace) override;

  // Set to true once setup for webcontent is initialized.
  bool has_web_content_setup_ = false;
  // views::WebDialogView that owns this delegate.
  raw_ptr<views::WebDialogView> web_dialog_view_;
  // views::Widget that owns this delegate.
  raw_ptr<views::Widget> widget_;
  // Delegate for accessing //chrome.
  base::WeakPtr<ShimlessRmaDelegate> shimless_rma_delegate_;
  // Callback for handling the console log from the app.
  ConsoleLogCallback on_console_log_;
};

}  // namespace ash::shimless_rma

#endif  // ASH_WEBUI_SHIMLESS_RMA_BACKEND_EXTERNAL_APP_DIALOG_H_
