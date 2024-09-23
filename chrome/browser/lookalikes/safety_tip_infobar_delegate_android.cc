// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tip_infobar_delegate_android.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/lookalikes/safety_tip_ui_helper.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

SafetyTipInfoBarDelegate::SafetyTipInfoBarDelegate(
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url,
    content::WebContents* web_contents,
    base::OnceCallback<void(SafetyTipInteraction)> close_callback)
    : safety_tip_status_(safety_tip_status),
      suggested_url_(suggested_url),
      close_callback_(std::move(close_callback)),
      web_contents_(web_contents) {}

SafetyTipInfoBarDelegate::~SafetyTipInfoBarDelegate() {
  std::move(close_callback_).Run(action_taken_);
}

std::u16string SafetyTipInfoBarDelegate::GetMessageText() const {
  return GetSafetyTipTitle(safety_tip_status_, suggested_url_);
}

int SafetyTipInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string SafetyTipInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  switch (button) {
    case BUTTON_OK:
      return l10n_util::GetStringUTF16(
          GetSafetyTipLeaveButtonId(safety_tip_status_));
    case BUTTON_CANCEL:
    case BUTTON_NONE:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

bool SafetyTipInfoBarDelegate::Accept() {
  action_taken_ = SafetyTipInteraction::kLeaveSite;
  auto url = safety_tip_status_ == security_state::SafetyTipStatus::kLookalike
                 ? suggested_url_
                 : GURL();
  LeaveSiteFromSafetyTip(web_contents_, url);
  return true;
}

infobars::InfoBarDelegate::InfoBarIdentifier
SafetyTipInfoBarDelegate::GetIdentifier() const {
  return SAFETY_TIP_INFOBAR_DELEGATE;
}

int SafetyTipInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_SAFETYTIP_SHIELD;
}

void SafetyTipInfoBarDelegate::InfoBarDismissed() {
  // Called when you click the X. Treat as 'ignore'.
  action_taken_ = SafetyTipInteraction::kDismissWithClose;
}

std::u16string SafetyTipInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_MORE_INFO_LINK);
}

bool SafetyTipInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  OpenHelpCenterFromSafetyTip(web_contents_);
  return false;
}

std::u16string SafetyTipInfoBarDelegate::GetDescriptionText() const {
  return GetSafetyTipDescription(safety_tip_status_, suggested_url_);
}
