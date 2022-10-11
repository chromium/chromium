// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_HATS_HATS_DIALOG_H_
#define CHROME_BROWSER_ASH_HATS_HATS_DIALOG_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

namespace ash {

// Happiness tracking survey dialog. Sometimes appears after login to ask the
// user how satisfied they are with their Chromebook.
// This class lives on the UI thread.
class HatsDialog : public ui::WebDialogDelegate {
 public:
  HatsDialog(const HatsDialog&) = delete;
  HatsDialog& operator=(const HatsDialog&) = delete;
  ~HatsDialog() override;

  static void Show(const std::string& trigger_id,
                   const std::string& histogram_name,
                   const std::string& site_context);

  // Based on the supplied |action|, returns true if the client should be
  // closed. Handling the action could imply logging or incrementing a survey
  // specific UMA metric (using |histogram_name|).
  static bool HandleClientTriggeredAction(const std::string& action,
                                          const std::string& histogram_name);

 private:
  FRIEND_TEST_ALL_PREFIXES(HatsDialogTest, ParseAnswer);

  // This class must be allocated on the heap, and general care should be taken
  // regarding its lifetime, due to its self-managing use of delete in the
  // `OnDialogClosed` method.
  HatsDialog(const std::string& trigger_id,
             const std::string& histogram_name,
             const std::string& site_context);

  static bool ParseAnswer(const std::string& input,
                          int* question,
                          std::vector<int>* scores);

  // ui::WebDialogDelegate implementation.
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnLoadingStateChanged(content::WebContents* source) override;
  bool ShouldShowDialogTitle() const override;
  bool ShouldShowCloseButton() const override;
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;
  ui::WebDialogDelegate::FrameKind GetWebDialogFrameKind() const override;

  const std::string trigger_id_;
  const std::string histogram_name_;
  std::string url_;

  std::string action_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_HATS_HATS_DIALOG_H_
