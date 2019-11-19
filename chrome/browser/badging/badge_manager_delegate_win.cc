// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager_delegate_win.h"

#include "base/i18n/number_formatting.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/taskbar/taskbar_decorator_win.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace badging {

namespace {

// Determines the badge contents and alt text.
// base::nullopt if the badge is not set.
// otherwise a pair (badge_content, badge_alt_text), based on the content of the
// badge.
base::Optional<std::pair<std::string, std::string>> GetBadgeContentAndAlt(
    const base::Optional<BadgeManager::BadgeValue>& badge) {
  // If there is no badge, there is no contents or alt text.
  if (!badge)
    return base::nullopt;

  std::string badge_string = badging::GetBadgeString(badge.value());
  // There are 3 different cases when the badge has a value:
  // 1. |contents| is between 1 and 99 inclusive => Set the accessibility text
  //    to a pluralized notification count (e.g. 4 Unread Notifications).
  // 2. |contents| is greater than 99 => Set the accessibility text to
  //    More than |kMaxBadgeContent| unread notifications, so the
  //    accessibility text matches what is displayed on the badge (e.g. More
  //    than 99 notifications).
  // 3. The badge is set to 'flag' => Set the accessibility text to something
  //    less specific (e.g. Unread Notifications).
  std::string badge_alt_string;
  if (badge.value()) {
    uint64_t value = badge.value().value();
    badge_alt_string = value <= badging::kMaxBadgeContent
                           // Case 1.
                           ? l10n_util::GetPluralStringFUTF8(
                                 IDS_BADGE_UNREAD_NOTIFICATIONS, value)
                           // Case 2.
                           : l10n_util::GetPluralStringFUTF8(
                                 IDS_BADGE_UNREAD_NOTIFICATIONS_SATURATED,
                                 badging::kMaxBadgeContent);
  } else {
    // Case 3.
    badge_alt_string =
        l10n_util::GetStringUTF8(IDS_BADGE_UNREAD_NOTIFICATIONS_UNSPECIFIED);
  }

  return std::make_pair(badge_string, badge_alt_string);
}

}  // namespace

BadgeManagerDelegateWin::BadgeManagerDelegateWin(Profile* profile,
                                                 BadgeManager* badge_manager)
    : BadgeManagerDelegate(profile, badge_manager) {}

void BadgeManagerDelegateWin::OnAppBadgeUpdated(const web_app::AppId& app_id) {
  const auto& content_and_alt =
      GetBadgeContentAndAlt(badge_manager()->GetBadgeValue(app_id));

  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!IsAppBrowser(browser, app_id))
      continue;

    auto* window = browser->window()->GetNativeWindow();
    if (content_and_alt) {
      taskbar::DrawTaskbarDecorationString(window, content_and_alt->first,
                                           content_and_alt->second);
    } else {
      taskbar::UpdateTaskbarDecoration(browser->profile(), window);
    }
  }
}

bool BadgeManagerDelegateWin::IsAppBrowser(Browser* browser,
                                           const std::string& app_id) {
  return browser->app_controller() &&
         browser->app_controller()->GetAppId() == app_id &&
         browser->profile() == profile();
}

}  // namespace badging
