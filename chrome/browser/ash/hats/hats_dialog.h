// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_HATS_HATS_DIALOG_H_
#define CHROME_BROWSER_ASH_HATS_HATS_DIALOG_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

class Profile;

namespace ash {
struct HatsConfig;

// Happiness tracking survey dialog. Sometimes appears after login to ask the
// user how satisfied they are with their Chromebook.
// This class lives on the UI thread.
class HatsDialog : public ui::WebDialogDelegate {
 public:
  // Creates an instance of HatsDialog and posts a task to load all the relevant
  // device info before displaying the dialog. If |product_specific_data| is
  // provided, the key-value pairs will be attached to the survey results.
  static std::unique_ptr<HatsDialog> CreateAndShow(
      const HatsConfig& hats_config,
      const base::flat_map<std::string, std::string>& product_specific_data =
          base::flat_map<std::string, std::string>());

  HatsDialog(const HatsDialog&) = delete;
  HatsDialog& operator=(const HatsDialog&) = delete;

  ~HatsDialog() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(HatsDialogTest, GetFormattedSiteContext);
  FRIEND_TEST_ALL_PREFIXES(HatsDialogTest, HandleClientTriggeredAction);

  void Show(const std::string& site_context);

  HatsDialog(const std::string& trigger_id,
             Profile* user_profile,
             const std::string& histogram_name);

  // Must be run on a blocking thread pool.
  // Gathers the browser version info, firmware info and platform info and
  // returns them in a single encoded string, in the format
  // "<key>=<value>&<key>=<value>&<key>=<value>" where the keys and values are
  // url-escaped. Any key-value pairs in |product_specific_data| are also
  // encoded and appended to the string, unless the keys collide with existing
  // device info keys.
  static std::string GetFormattedSiteContext(
      const std::string& user_locale,
      const base::flat_map<std::string, std::string>& product_specific_data);

  // Based on the supplied |action|, returns true if the client should be
  // closed. Handling the action could imply logging or incrementing a survey
  // specific UMA metric (using |histogram_name|).
  static bool HandleClientTriggeredAction(const std::string& action,
                                          const std::string& histogram_name);

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
  std::string url_;
  Profile* user_profile_;
  const std::string histogram_name_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_HATS_HATS_DIALOG_H_
