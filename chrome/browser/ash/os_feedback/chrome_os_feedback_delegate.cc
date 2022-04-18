// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.h"

#include <string>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace ash {

ChromeOsFeedbackDelegate::ChromeOsFeedbackDelegate(Profile* profile)
    : profile_(profile) {
  Browser* browser = BrowserList::GetInstance()->GetLastActive();
  if (browser) {
    // Save the last active page url before opening the feedback tool.
    page_url_ = chrome::GetTargetTabUrl(
        browser->session_id(), browser->tab_strip_model()->active_index());
  }
}

ChromeOsFeedbackDelegate::~ChromeOsFeedbackDelegate() = default;

std::string ChromeOsFeedbackDelegate::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

absl::optional<GURL> ChromeOsFeedbackDelegate::GetLastActivePageUrl() {
  return page_url_;
}

absl::optional<std::string> ChromeOsFeedbackDelegate::GetSignedInUserEmail()
    const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile_);
  if (!identity_manager)
    return absl::nullopt;
  // Browser sync consent is not required to use feedback.
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

}  // namespace ash
