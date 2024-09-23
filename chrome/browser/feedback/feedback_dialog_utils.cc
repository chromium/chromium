// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_dialog_utils.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/account_id/account_id.h"
#endif

namespace chrome {

GURL GetTargetTabUrl(SessionID session_id, int index) {
  Browser* browser = chrome::FindBrowserWithID(session_id);
  // Sanity checks.
  if (!browser || index >= browser->tab_strip_model()->count())
    return GURL();

  if (index >= 0) {
    content::WebContents* target_tab =
        browser->tab_strip_model()->GetWebContentsAt(index);
    if (target_tab) {
      if (browser->is_type_devtools()) {
        if (auto* dev_tools_window =
                DevToolsWindow::AsDevToolsWindow(target_tab)) {
          target_tab = dev_tools_window->GetInspectedWebContents();
        }
      }
      if (target_tab)
        return target_tab->GetLastCommittedURL();
    }
  }

  return GURL();
}

Profile* GetFeedbackProfile(const Browser* browser) {
  Profile* profile =
      browser ? browser->profile()
              : ProfileManager::GetLastUsedProfileAllowedByPolicy();
  if (!profile)
    return nullptr;

  // We do not want to launch on an OTR profile.
  profile = profile->GetOriginalProfile();
  DCHECK(profile);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Obtains the display profile ID on which the Feedback window should show.
  auto* const window_manager = MultiUserWindowManagerHelper::GetWindowManager();
  const AccountId display_account_id =
      window_manager && browser ? window_manager->GetUserPresentingWindow(
                                      browser->window()->GetNativeWindow())
                                : EmptyAccountId();
  if (display_account_id.is_valid())
    profile = multi_user_util::GetProfileFromAccountId(display_account_id);
#endif
  return profile;
}

void ShowFeedbackDialogForWebUI(WebUIFeedbackSource webui_source,
                                const std::string& extra_diagnostics) {
  feedback::FeedbackSource source;
  std::string category;
  switch (webui_source) {
    case WebUIFeedbackSource::kConnectivityDiagnostics:
      source = feedback::FeedbackSource::kFeedbackSourceConnectivityDiagnostics;
      category = "connectivity-diagnostics";
      break;
  }

  ShowFeedbackPage(nullptr, source,
                   /*description_template=*/std::string(),
                   /*description_placeholder_text=*/std::string(), category,
                   extra_diagnostics);
}

}  // namespace chrome
