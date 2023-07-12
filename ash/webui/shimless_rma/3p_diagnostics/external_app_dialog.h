// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_SHIMLESS_RMA_3P_DIAGNOSTICS_EXTERNAL_APP_DIALOG_H_
#define ASH_WEBUI_SHIMLESS_RMA_3P_DIAGNOSTICS_EXTERNAL_APP_DIALOG_H_

#include "base/memory/raw_ptr.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash::shimless_rma {

// Provides a dialog to show external apps on shimless rma screen. It creates a
// WebDialogView, and disables some API (e.g. opening tabs, opening file
// chooser) to prevent them being used in shimless rma screen.
class ExternalAppDialog : public ui::WebDialogDelegate {
 public:
  // Params for a dialog.
  struct InitParams {
    // The BrowserContext for the dialog.
    base::raw_ptr<content::BrowserContext> context;
    // The url of the dialog content.
    const GURL& content_url;
  };

  // Shows the dialog. Shouldn't be called if last dialog opened by this
  // function is still open.
  static void Show(const InitParams& params);

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
  bool OnDialogCloseRequested() override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  std::string GetDialogArgs() const override;
  // NOTE: This function deletes this object at the end.
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldCloseDialogOnEscape() const override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldCenterDialogTitleText() const override;
  bool ShouldShowCloseButton() const override;

  // The url of the content.
  GURL content_url_;
};

}  // namespace ash::shimless_rma

#endif  // ASH_WEBUI_SHIMLESS_RMA_3P_DIAGNOSTICS_EXTERNAL_APP_DIALOG_H_
