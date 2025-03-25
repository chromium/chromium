// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_settings_util.h"

#include "chrome/browser/glic/resources/grit/glic_browser_resources.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"

namespace {

void OpenGlicSettingsPageWithPromo(Profile* profile,
                                   const base::Feature& feature,
                                   ShowPromoInPage::Params promo_params) {
  Browser* browser = chrome::FindTabbedBrowser(profile, false);
  if (!browser) {
    // At this point we don't have a browser window open for profile.
    // User Education resources are initialized when browser view is created,
    // so create a browser window prior to using the service
    browser = Browser::Create(Browser::CreateParams(profile, true));
  }

  const bool show_promo_bubble =
      UserEducationService::MaybeShowNewBadge(profile, feature);
  if (show_promo_bubble) {
    promo_params.target_url =
        chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage);
    promo_params.page_open_mode = user_education::PageOpenMode::kSingletonTab;
    ShowPromoInPage::Start(browser, std::move(promo_params));
  } else {
    glic::OpenGlicSettingsPage(profile);
  }
}

}  // namespace

namespace glic {

void OpenGlicSettingsPage(Profile* profile) {
  NavigateParams params(profile,
                        chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params.disposition = WindowOpenDisposition::SINGLETON_TAB;
  Navigate(&params);
}

void OpenGlicOsToggleSetting(Profile* profile) {
  ShowPromoInPage::Params params;
  params.bubble_anchor_id = kGlicOsToggleElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomRight;
  params.bubble_text =
      l10n_util::GetStringUTF16(IDS_GLIC_OS_WIDGET_TOGGLE_HELP_BUBBLE);

  OpenGlicSettingsPageWithPromo(profile, features::kGlic, std::move(params));
}

void OpenGlicKeyboardShortcutSetting(Profile* profile) {
  ShowPromoInPage::Params params;
  params.bubble_anchor_id = kGlicOsWidgetKeyboardShortcutElementId;
  params.bubble_arrow = user_education::HelpBubbleArrow::kBottomRight;
  params.bubble_text = l10n_util::GetStringUTF16(
      IDS_GLIC_OS_WIDGET_KEYBOARD_SHORTCUT_HELP_BUBBLE);

  OpenGlicSettingsPageWithPromo(
      profile, features::kGlicKeyboardShortcutNewBadge, std::move(params));
}

}  // namespace glic
