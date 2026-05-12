// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/common/glic_navigation.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/user_education/show_promo_in_page.h"
#include "chrome/browser/user_education/user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_education/common/help_bubble/help_bubble_params.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace {

void OpenGlicSettingsPageWithPromo(Profile* profile,
                                   const base::Feature& feature,
                                   ShowPromoInPage::Params promo_params) {
  BrowserWindowInterface* browser =
      ProfileBrowserCollection::GetForProfile(profile)->FindTabbedBrowser();
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
    ShowPromoInPage::Start(browser->GetBrowserForMigrationOnly(),
                           std::move(promo_params));
  } else {
    glic::OpenGlicSettingsPage(profile);
  }
}

}  // namespace

namespace glic {

void OpenGlicSettingsPage(Profile* profile) {
  auto params = std::make_unique<NavigateParams>(
      profile, chrome::GetSettingsUrl(chrome::kGlicSettingsSubpage),
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::SINGLETON_TAB;
  glic::Navigate(std::move(params));
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

void OpenPasswordManagerSettingsPage(Profile* profile) {
  const GURL settings_url =
      base::FeatureList::IsEnabled(features::kFedCmEmbedderInitiatedLogin)
          ? chrome::GetSettingsUrl(chrome::kGlicLoginSettingsSubpage)
          : GURL(GetGooglePasswordManagerSubPageURLStr());
  auto params = std::make_unique<NavigateParams>(
      profile, settings_url, ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  params->disposition = WindowOpenDisposition::SINGLETON_TAB;
  glic::Navigate(std::move(params));
}

std::string_view GetPlatformHelpSuffix() {
#if BUILDFLAG(IS_WIN)
  return "_win";
#elif BUILDFLAG(IS_MAC)
  return "_mac";
#elif BUILDFLAG(IS_CHROMEOS)
  return "_chromeos";
#elif BUILDFLAG(IS_LINUX)
  return "_linux";
#else
  return "";
#endif
}

}  // namespace glic
