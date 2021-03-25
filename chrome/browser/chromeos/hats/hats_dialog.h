// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_HATS_HATS_DIALOG_H_
#define CHROME_BROWSER_CHROMEOS_HATS_HATS_DIALOG_H_

#include <string>

#include "base/macros.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

namespace chromeos {
struct HatsConfig;

// Happiness tracking survey dialog. Sometimes appears after login to ask the
// user how satisfied they are with their Chromebook.
// This class lives on the UI thread.
class HatsDialog : public ui::WebDialogDelegate {
 public:
  // Creates an instance of HatsDialog and posts a task to load all the relevant
  // device info before displaying the dialog.
  static std::unique_ptr<HatsDialog> CreateAndShow(
      const HatsConfig& hats_config);
  ~HatsDialog() override;

 private:
  void Show(const std::string& site_context);

  explicit HatsDialog(const std::string& trigger_id, Profile* user_profile);

  // ui::WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool HandleContextMenu(content::RenderFrameHost* render_frame_host,
                         const content::ContextMenuParams& params) override;
  ui::WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;

  const std::string trigger_id_;
  std::string url_;
  Profile* user_profile_;

  DISALLOW_COPY_AND_ASSIGN(HatsDialog);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_HATS_HATS_DIALOG_H_
