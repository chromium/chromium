// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"
#include "ui/gfx/range/range.h"

namespace safe_browsing {

// The |SettingsResetPromptController| class is responsible for providing the
// text that will be displayed in the settings reset dialog. The controller's
// |Accept()| and |Cancel()| functions will be called based on user
// interaction. Objects of this class will delete themselves after |Accept()| or
// |Cancel()| has been called.
class SettingsResetPromptController {
 public:
  // A controller should be created only if |model->ShouldPromptforReset()|
  // is true.
  SettingsResetPromptController(
      std::unique_ptr<SettingsResetPromptModel> model,
      std::unique_ptr<BrandcodedDefaultSettings> default_settings);

  base::string16 GetWindowTitle() const;
  base::string16 GetMainText() const;
  // Returns the offset into the main text string where a URL was inserted. To
  // be used by the dialog to apply an appropriate style to the URL text.
  gfx::Range GetMainTextUrlRange() const;
  // |DialogShown()| will be called by the dialog when the dialog's |Show()|
  // method is called. This allows the controller to notify the model that the
  // dialog has been shown so that preferences can be updated.
  void DialogShown();
  // |Accept()| will be called by the dialog when the user clicks the main
  // button, after which the dialog will be closed.
  void Accept();
  // |Cancel()| will be called by the dialog when the user clicks the cancel
  // button, after which the dialog will be closed.
  void Cancel();
  // |Close()| will be called by the dialog when the user dismisses the dialog,
  // for example by clicking the x in the top right corner or by pressing the
  // Escape key.
  void Close();

 private:
  ~SettingsResetPromptController();
  void InitMainText();
  // Function to be called sometime after |Accept()| or |Cancel()| has been
  // called to perform any final tasks (such as metrcis reporting) and delete
  // this object.
  void OnInteractionDone();

  std::unique_ptr<SettingsResetPromptModel> model_;
  std::unique_ptr<BrandcodedDefaultSettings> default_settings_;
  base::string16 main_text_;
  gfx::Range main_text_url_range_;

  // Used for metrics reporting.
  base::Time time_dialog_shown_;

  DISALLOW_COPY_AND_ASSIGN(SettingsResetPromptController);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_CONTROLLER_H_
