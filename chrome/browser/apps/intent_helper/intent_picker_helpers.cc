// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

namespace {

std::vector<IntentPickerAppInfo> FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
#if BUILDFLAG(IS_MAC)
  // On the Mac, if there is a Universal Link, it goes first.
  if (absl::optional<IntentPickerAppInfo> mac_app = FindMacAppForUrl(url))
    apps.push_back(std::move(mac_app.value()));
#endif  // BUILDFLAG(IS_MAC)

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browsers=*/true);

  for (const std::string& app_id : app_ids) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&apps](const AppUpdate& update) {
          apps.emplace(apps.begin(), GetPickerEntryType(update.AppType()),
                       ui::ImageModel(), update.AppId(), update.Name());
        });
  }
  return apps;
}

void LaunchAppFromIntentPicker(content::WebContents* web_contents,
                               const GURL& url,
                               const std::string& launch_name,
                               PickerEntryType app_type) {
#if BUILDFLAG(IS_CHROMEOS)
  LaunchAppFromIntentPickerChromeOs(web_contents, url, launch_name, app_type);
#else
  if (base::FeatureList::IsEnabled(features::kLinkCapturingUiUpdate)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    IntentPickerAutoDisplayService::Get(profile)->ResetIntentChipCounter(url);
  }

  switch (app_type) {
    case PickerEntryType::kWeb:
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
      break;
    case PickerEntryType::kMacOs:
#if BUILDFLAG(IS_MAC)
      LaunchMacApp(url, launch_name);
      break;
#endif  // BUILDFLAG(IS_MAC)
    case PickerEntryType::kArc:
    case PickerEntryType::kDevice:
    case PickerEntryType::kUnknown:
      NOTREACHED();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
#if BUILDFLAG(IS_CHROMEOS)
  OnIntentPickerClosedChromeOs(web_contents, ui_auto_display_service,
                               PickerShowState::kOmnibox, url, launch_name,
                               entry_type, close_reason, should_persist);
#else
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;
  if (should_launch_app) {
    LaunchAppFromIntentPicker(web_contents, url, launch_name, entry_type);
  }

  if (entry_type == PickerEntryType::kUnknown &&
      close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED &&
      ui_auto_display_service) {
    // We reach here if the picker was closed without an app being chosen, e.g.
    // due to the tab being closed. Keep count of this scenario so we can stop
    // the UI from showing after 2+ dismissals.
    ui_auto_display_service->IncrementPickerUICounter(url);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void OnAppIconsLoaded(content::WebContents* web_contents,
                      IntentPickerAutoDisplayService* ui_auto_display_service,
                      const GURL& url,
                      std::vector<IntentPickerAppInfo> apps) {
  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
#if BUILDFLAG(IS_CHROMEOS)
      /*show_stay_in_chrome=*/true,
      /*show_remember_selection=*/true,
#else
      /*show_stay_in_chrome=*/false,
      /*show_remember_selection=*/false,
#endif  // BUILDFLAG(IS_CHROMEOS)
      base::BindOnce(&OnIntentPickerClosed, web_contents,
                     ui_auto_display_service, url));
}

std::vector<IntentPickerAppInfo> GetAppsForIntentPicker(
    content::WebContents* web_contents) {
  std::vector<IntentPickerAppInfo> apps = {};
  if (!ShouldCheckAppsForUrl(web_contents))
    return apps;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile))
    return apps;

  const GURL& url = web_contents->GetLastCommittedURL();
  apps = FindAppsForUrl(web_contents, url, std::move(apps));
  return apps;
}

}  // namespace

// for chromeos, this should apply when navigation is not deferred for pwa only
// case also when navigation deferred and then resumed
bool MaybeShowIntentPicker(content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  std::vector<IntentPickerAppInfo> apps = GetAppsForIntentPicker(web_contents);
  bool show_intent_icon = !apps.empty();
#if BUILDFLAG(IS_CHROMEOS)
  MaybeShowIntentPickerBubble(navigation_handle, std::move(apps));
#endif  // BUILDFLAG(IS_CHROMEOS)
  return show_intent_icon;
}

void MaybeShowIntentPicker(content::WebContents* web_contents) {
  std::vector<IntentPickerAppInfo> apps = GetAppsForIntentPicker(web_contents);
  IntentPickerTabHelper::SetShouldShowIcon(web_contents, !apps.empty());
}

void ShowIntentPickerBubble(content::WebContents* web_contents,
                            const GURL& url) {
  std::vector<IntentPickerAppInfo> apps = FindAppsForUrl(web_contents, url, {});
  if (apps.empty())
    return;

#if BUILDFLAG(IS_CHROMEOS)
  apps::IntentHandlingMetrics::RecordIntentPickerIconEvent(
      apps::IntentHandlingMetrics::IntentPickerIconEvent::kIconClicked);
#endif

  if (apps.size() == 1 && apps::features::ShouldIntentChipSkipIntentPicker()) {
    LaunchAppFromIntentPicker(web_contents, url, apps[0].launch_name,
                              apps[0].type);
    return;
  }

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps),
      base::BindOnce(&OnAppIconsLoaded, web_contents,
                     /*ui_auto_display_service=*/nullptr, url));
}

bool IntentPickerPwaPersistenceEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

}  // namespace apps
