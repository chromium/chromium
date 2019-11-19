// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/reputation/safety_tip_ui_helper.h"

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

const char kSafetyTipLeaveSiteUrl[] = "chrome://newtab";

void RecordSafetyTipInteractionHistogram(content::WebContents* web_contents,
                                         SafetyTipInteraction interaction) {
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  base::UmaHistogramEnumeration(
      security_state::GetSafetyTipHistogramName(
          "Security.SafetyTips.Interaction",
          helper->GetVisibleSecurityState()->safety_tip_info.status),
      interaction);
}

void LeaveSiteFromSafetyTip(content::WebContents* web_contents,
                            const GURL& safe_url) {
  RecordSafetyTipInteractionHistogram(web_contents,
                                      SafetyTipInteraction::kLeaveSite);
  content::OpenURLParams params(
      safe_url, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */);
  params.should_replace_current_entry = true;
  web_contents->OpenURL(params);
}

void OpenHelpCenterFromSafetyTip(content::WebContents* web_contents) {
  RecordSafetyTipInteractionHistogram(web_contents,
                                      SafetyTipInteraction::kLearnMore);
  web_contents->OpenURL(content::OpenURLParams(
      GURL(chrome::kSafetyTipHelpCenterURL), content::Referrer(),
      WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
      false /*is_renderer_initiated*/));
}

base::string16 GetSafetyTipTitle(
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url) {
  switch (safety_tip_status) {
    case security_state::SafetyTipStatus::kBadReputation:
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_TITLE);
    case security_state::SafetyTipStatus::kLookalike:
#if defined(OS_ANDROID)
      return l10n_util::GetStringUTF16(IDS_SAFETY_TIP_ANDROID_LOOKALIKE_TITLE);
#else
      return l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_TITLE,
          security_interstitials::common_string_util::GetFormattedHostName(
              suggested_url));
#endif
    case security_state::SafetyTipStatus::kBadReputationIgnored:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kBadKeyword:
    case security_state::SafetyTipStatus::kUnknown:
    case security_state::SafetyTipStatus::kNone:
      NOTREACHED();
  }

  NOTREACHED();
  return base::string16();
}

base::string16 GetSafetyTipDescription(
    security_state::SafetyTipStatus warning_type,
    const GURL& url,
    const GURL& suggested_url) {
  switch (warning_type) {
    case security_state::SafetyTipStatus::kBadReputation:
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_SAFETY_TIP_BAD_REPUTATION_DESCRIPTION);
    case security_state::SafetyTipStatus::kLookalike:
#if defined(OS_ANDROID)
      return l10n_util::GetStringFUTF16(
          IDS_SAFETY_TIP_ANDROID_LOOKALIKE_DESCRIPTION,
          security_interstitials::common_string_util::GetFormattedHostName(url),
          security_interstitials::common_string_util::GetFormattedHostName(
              suggested_url));
#else
      return l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_DESCRIPTION,
          security_interstitials::common_string_util::GetFormattedHostName(
              suggested_url));
#endif
    case security_state::SafetyTipStatus::kBadReputationIgnored:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kBadKeyword:
    case security_state::SafetyTipStatus::kNone:
    case security_state::SafetyTipStatus::kUnknown:
      NOTREACHED();
  }
  NOTREACHED();
  return base::string16();
}

int GetSafetyTipLeaveButtonId(security_state::SafetyTipStatus warning_type) {
  switch (warning_type) {
#if defined(OS_ANDROID)
    case security_state::SafetyTipStatus::kBadReputation:
    case security_state::SafetyTipStatus::kLookalike:
      return IDS_SAFETY_TIP_ANDROID_LEAVE_BUTTON;
#else
    case security_state::SafetyTipStatus::kBadReputation:
    case security_state::SafetyTipStatus::kLookalike:
      return IDS_PAGE_INFO_SAFETY_TIP_LEAVE_BUTTON;
#endif
    case security_state::SafetyTipStatus::kBadReputationIgnored:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kBadKeyword:
    case security_state::SafetyTipStatus::kUnknown:
    case security_state::SafetyTipStatus::kNone:
      NOTREACHED();
      return 0;
  }
  NOTREACHED();
  return 0;
}
