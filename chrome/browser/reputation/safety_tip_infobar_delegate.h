// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REPUTATION_SAFETY_TIP_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_REPUTATION_SAFETY_TIP_INFOBAR_DELEGATE_H_

#include "chrome/browser/reputation/safety_tip_ui.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

class SafetyTipInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SafetyTipInfoBarDelegate(
      security_state::SafetyTipStatus safety_tip_status,
      const GURL& suggested_url,
      content::WebContents* web_contents,
      base::OnceCallback<void(SafetyTipInteraction)> close_callback);
  ~SafetyTipInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  // infobars::InfoBarDelegate
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  void InfoBarDismissed() override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

  // This function is the equivalent of GetMessageText(), but for the portion of
  // the infobar below the 'message' title.
  base::string16 GetDescriptionText() const;

 private:
  security_state::SafetyTipStatus safety_tip_status_;

  // The URL of the page the Safety Tip suggests you intended to go to, when
  // applicable (for SafetyTipStatus::kLookalike).
  const GURL suggested_url_;

  SafetyTipInteraction action_taken_ = SafetyTipInteraction::kNoAction;
  base::OnceCallback<void(SafetyTipInteraction)> close_callback_;
  content::WebContents* web_contents_;
};

#endif  // CHROME_BROWSER_REPUTATION_SAFETY_TIP_INFOBAR_DELEGATE_H_
