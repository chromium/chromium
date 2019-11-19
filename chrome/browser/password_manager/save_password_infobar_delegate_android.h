// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_SAVE_PASSWORD_INFOBAR_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_SAVE_PASSWORD_INFOBAR_DELEGATE_ANDROID_H_

#include <memory>

#include "base/macros.h"
#include "base/timer/elapsed_timer.h"
#include "chrome/browser/password_manager/password_manager_infobar_delegate_android.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/password_manager/core/browser/password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_onboarding.h"
#include "ui/gfx/range/range.h"

namespace content {
class WebContents;
}

// After a successful *new* login attempt, we take the PasswordFormManager in
// provisional_save_manager_ and move it to a SavePasswordInfoBarDelegate while
// the user makes up their mind with the "save password" infobar. Note if the
// login is one we already know about, the end of the line is
// provisional_save_manager_ because we just update it on success and so such
// forms never end up in an infobar.
class SavePasswordInfoBarDelegate : public PasswordManagerInfoBarDelegate {
 public:
  // If we won't be showing the one-click signin infobar, creates a save
  // password infobar and delegate and adds the infobar to the InfoBarService
  // for |web_contents|.
  static void Create(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      std::unique_ptr<password_manager::SavingFlowMetricsRecorder>
          saving_flow_recorder);

  ~SavePasswordInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 protected:
  // Makes a ctor available in tests.
  SavePasswordInfoBarDelegate(
      content::WebContents* web_contents,
      std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save,
      bool is_smartlock_branding_enabled,
      std::unique_ptr<password_manager::SavingFlowMetricsRecorder>
          saving_flow_recorder);

 private:
  // The PasswordFormManager managing the form we're asking the user about,
  // and should update as per their decision.
  std::unique_ptr<password_manager::PasswordFormManagerForUI> form_to_save_;

  // Used to track the results we get from the info bar.
  password_manager::metrics_util::UIDismissalReason infobar_response_;

  // Measures the "Save password?" prompt lifetime. Used to report an UMA
  // signal.
  base::ElapsedTimer timer_;

  std::unique_ptr<password_manager::SavingFlowMetricsRecorder>
      saving_flow_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordInfoBarDelegate);
};

// Creates the platform-specific SavePassword InfoBar. This function is defined
// in platform-specific .cc (or .mm) files.
std::unique_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    std::unique_ptr<SavePasswordInfoBarDelegate> delegate);

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_SAVE_PASSWORD_INFOBAR_DELEGATE_ANDROID_H_
