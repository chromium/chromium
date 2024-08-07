// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lookalikes/safety_tip_ui_helper.h"

#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "components/security_interstitials/core/common_string_util.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "content/public/browser/navigation_controller.h"
#endif

namespace {

// URL that the "leave site" button aborts to by default.
const char kSafetyTipLeaveSiteUrl[] = "chrome://newtab";

}  // namespace

void LeaveSiteFromSafetyTip(content::WebContents* web_contents,
                            const GURL& safe_url) {
  auto navigated_to = safe_url;
  if (navigated_to.is_empty()) {
    navigated_to = GURL(kSafetyTipLeaveSiteUrl);

#if BUILDFLAG(IS_ANDROID)
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
    if (tab && tab->IsCustomTab()) {
      auto& controller = web_contents->GetController();
      // For CCTs, just go back if we can...
      if (controller.CanGoBack()) {
        controller.GoBack();
        return;
      }
      // ... or close the CCT otherwise.
      auto* tab_model = TabModelList::GetTabModelForWebContents(web_contents);
      if (tab_model) {
        tab_model->CloseTabAt(tab_model->GetActiveIndex());
        return;
      }
      // (And if we don't have a tab model for some reason, just navigate away
      //  someplace at least slightly. To my knowledge, this shouldn't happen.)
    }
#endif
  }

  content::OpenURLParams params(
      navigated_to, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */);
  params.should_replace_current_entry = true;
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
}

void OpenHelpCenterFromSafetyTip(content::WebContents* web_contents) {
  web_contents->OpenURL(
      content::OpenURLParams(
          GURL(chrome::kSafetyTipHelpCenterURL), content::Referrer(),
          WindowOpenDisposition::NEW_FOREGROUND_TAB, ui::PAGE_TRANSITION_LINK,
          false /*is_renderer_initiated*/),
      /*navigation_handle_callback=*/{});
}

std::u16string GetSafetyTipTitle(
    security_state::SafetyTipStatus safety_tip_status,
    const GURL& suggested_url) {
  switch (safety_tip_status) {
    case security_state::SafetyTipStatus::kLookalike:
      return l10n_util::GetStringFUTF16(
          IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_TITLE,
          security_interstitials::common_string_util::GetFormattedHostName(
              suggested_url));
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kUnknown:
    case security_state::SafetyTipStatus::kNone:
      NOTREACHED_IN_MIGRATION();
  }

  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string GetSafetyTipDescription(
    security_state::SafetyTipStatus warning_type,
    const GURL& suggested_url) {
  switch (warning_type) {
    case security_state::SafetyTipStatus::kLookalike:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_SAFETY_TIP_DESCRIPTION);
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kNone:
    case security_state::SafetyTipStatus::kUnknown:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

int GetSafetyTipLeaveButtonId(security_state::SafetyTipStatus warning_type) {
  switch (warning_type) {
    case security_state::SafetyTipStatus::kLookalike:
      return IDS_PAGE_INFO_SAFETY_TIP_LOOKALIKE_LEAVE_BUTTON;
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kUnknown:
    case security_state::SafetyTipStatus::kNone:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
  NOTREACHED_IN_MIGRATION();
  return 0;
}
