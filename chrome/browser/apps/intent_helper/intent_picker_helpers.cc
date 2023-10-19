// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"

#include <string>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"
#include "chrome/browser/apps/link_capturing/intent_picker_info.h"
#include "chrome/browser/apps/link_capturing/link_capturing_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_attempt.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/link_capturing/chromeos_apps_intent_picker_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/apps/link_capturing/mac_intent_picker_helpers.h"
#endif

namespace apps {

int GetIntentPickerBubbleIconSize() {
  constexpr int kIntentPickerUiUpdateIconSize = 40;

  return features::LinkCapturingUiUpdateEnabled()
             ? kIntentPickerUiUpdateIconSize
             : gfx::kFaviconSize;
}

void LaunchAppFromIntentPicker(content::WebContents* web_contents,
                               const GURL& url,
                               const std::string& launch_name,
                               apps::PickerEntryType app_type) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  CHECK(profile);
  if (base::FeatureList::IsEnabled(apps::features::kLinkCapturingUiUpdate)) {
    IntentChipDisplayPrefs::ResetIntentChipCounter(profile, url);
  }

#if BUILDFLAG(IS_CHROMEOS)
  ChromeOsAppsIntentPickerDelegate intent_delegate(profile);
  intent_delegate.LaunchApp(web_contents, url, launch_name, app_type);
#else
  switch (app_type) {
    case apps::PickerEntryType::kWeb:
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
      if (base::FeatureList::IsEnabled(features::kDesktopPWAsLinkCapturing)) {
        web_app::WebAppProvider* web_app_provider =
            web_app::WebAppProvider::GetForWebApps(profile);
        CHECK(web_app_provider);
        web_app_provider->ui_manager().MaybeCreateEnableSupportedLinksInfobar(
            web_contents, launch_name);
      }
      break;
    case apps::PickerEntryType::kMacOs:
#if BUILDFLAG(IS_MAC)
      LaunchMacApp(url, launch_name);
      break;
#endif  // BUILDFLAG(IS_MAC)
    case apps::PickerEntryType::kArc:
    case apps::PickerEntryType::kDevice:
    case apps::PickerEntryType::kUnknown:
      NOTREACHED();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace apps
