// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

void OpenGlicSettingsPage(Profile* profile) {
  NavigateParams params(profile,
                        chrome::GetSettingsUrl(chrome::kChromeUIGlicHost),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

void OpenGlicOsToggleSetting(Profile* profile) {
  ShowPromoInPage::Params params;
  params.bubble_anchor_id = kGlicOsToggleElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomRight;
  params.bubble_text =
      l10n_util::GetStringUTF16(IDS_GLIC_OS_WIDGET_TOGGLE_HELP_BUBBLE);
  chrome::ShowPageWithPromoForProfile(
      profile, chrome::GetSettingsUrl(chrome::kChromeUIGlicHost),
      std::move(params));
}

void OpenGlicKeyboardShortcutSetting(Profile* profile) {
  ShowPromoInPage::Params params;
  params.bubble_anchor_id = kGlicOsWidgetKeyboardShortcutElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomRight;
  params.bubble_text = l10n_util::GetStringUTF16(
      IDS_GLIC_OS_WIDGET_KEYBOARD_SHORTCUT_HELP_BUBBLE);
  chrome::ShowPageWithPromoForProfile(
      profile, chrome::GetSettingsUrl(chrome::kChromeUIGlicHost),
      std::move(params));
}

}  // namespace glic
