// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#include "chrome/browser/apps/intent_helper/supported_links_infobar_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/display/types/display_constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps {

namespace {

void CloseOrGoBack(content::WebContents* web_contents) {
  DCHECK(web_contents);
  if (web_contents->GetController().CanGoBack()) {
    web_contents->GetController().GoBack();
  } else {
    web_contents->ClosePage();
  }
}

}  // namespace

void OnIntentPickerClosedChromeOs(
    base::WeakPtr<content::WebContents> web_contents,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  if (!web_contents) {
    return;
  }

  if (should_persist) {
    DCHECK(!launch_name.empty());

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);
    DCHECK(proxy);
    proxy->SetSupportedLinksPreference(launch_name);
    apps::IntentHandlingMetrics::RecordLinkCapturingEvent(
        entry_type,
        apps::IntentHandlingMetrics::LinkCapturingEvent::kSettingsChanged);
  }

  if (close_reason == IntentPickerCloseReason::OPEN_APP) {
    LaunchAppFromIntentPickerChromeOs(web_contents.get(), url, launch_name,
                                      entry_type);
  }

  IntentHandlingMetrics::RecordIntentPickerMetrics(entry_type, close_reason,
                                                   should_persist);
}

void LaunchAppFromIntentPickerChromeOs(content::WebContents* web_contents,
                                       const GURL& url,
                                       const std::string& launch_name,
                                       PickerEntryType app_type) {
  DCHECK(!launch_name.empty());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  if (base::FeatureList::IsEnabled(features::kLinkCapturingUiUpdate)) {
    IntentChipDisplayPrefs::ResetIntentChipCounter(profile, url);
  }

  apps::IntentHandlingMetrics::RecordLinkCapturingEvent(
      app_type, apps::IntentHandlingMetrics::LinkCapturingEvent::kAppOpened);

  if (app_type == PickerEntryType::kWeb) {
    web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);

    if (features::LinkCapturingInfoBarEnabled()) {
      SupportedLinksInfoBarDelegate::MaybeShowSupportedLinksInfoBar(
          web_contents, launch_name);
    }
  } else {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

    // TODO(crbug.com/853604): Distinguish the source from link and omnibox.
    proxy->LaunchAppWithUrl(
        launch_name,
        GetEventFlags(WindowOpenDisposition::NEW_WINDOW,
                      /*prefer_container=*/true),
        url, LaunchSource::kFromLink,
        std::make_unique<WindowInfo>(display::kDefaultDisplayId));
    CloseOrGoBack(web_contents);
  }
}

}  // namespace apps
