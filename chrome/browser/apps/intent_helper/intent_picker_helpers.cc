// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"

#include <string>
#include <utility>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_service.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/apps/intent_helper/ash_intent_picker_helpers.h"
#elif defined(OS_MAC)
#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"
#endif

namespace apps {

namespace {

std::vector<IntentPickerAppInfo> FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    std::vector<IntentPickerAppInfo> apps) {
#if defined(OS_MAC)
  apps = FindMacAppsForUrl(web_contents, url, std::move(apps));
#endif
  return FindPwaForUrl(web_contents, url, std::move(apps));
}

void OnIntentPickerClosed(
    content::WebContents* web_contents,
    IntentPickerAutoDisplayService* ui_auto_display_service,
    const GURL& url,
    const std::string& launch_name,
    PickerEntryType entry_type,
    IntentPickerCloseReason close_reason,
    bool should_persist) {
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;
  switch (entry_type) {
    case PickerEntryType::kWeb:
      if (should_launch_app)
        web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
      break;
    case apps::PickerEntryType::kUnknown:
      // We reach here if the picker was closed without an app being chosen,
      // e.g. due to the tab being closed. Keep count of this scenario so we can
      // stop the UI from showing after 2+ dismissals.
      if (close_reason == IntentPickerCloseReason::DIALOG_DEACTIVATED) {
        if (ui_auto_display_service)
          ui_auto_display_service->IncrementCounter(url);
      }
      break;
    case PickerEntryType::kMacOs:
#if defined(OS_MAC)
      if (should_launch_app) {
        LaunchMacApp(url, launch_name);
      }
      break;
#endif
    case PickerEntryType::kArc:
    case PickerEntryType::kDevice:
      NOTREACHED();
  }
}

}  // namespace

// for chromeos, this should apply when navigation is not deferred for pwa only
// case also when navigation deferred and then resumed
void MaybeShowIntentPicker(content::NavigationHandle* navigation_handle) {
  content::WebContents* web_contents = navigation_handle->GetWebContents();
  if (!ShouldCheckAppsForUrl(web_contents)) {
    return;
  }

// TODO(crbug.com/824598): Store the apps that is found in
// intent_picker_tab_helper and update the icon visibility and apps if there is
// app installed or uninstalled.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  MaybeShowIntentPickerAsh(navigation_handle);
#else
  const GURL& url = navigation_handle->GetURL();
  std::vector<IntentPickerAppInfo> apps = FindAppsForUrl(web_contents, url, {});
  IntentPickerTabHelper::SetShouldShowIcon(web_contents, !apps.empty());
#endif
}

void ShowIntentPickerBubble(content::WebContents* web_contents,
                            const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ShowIntentPickerBubbleAsh(web_contents, url);
#else
  std::vector<IntentPickerAppInfo> apps = FindAppsForUrl(web_contents, url, {});

  if (apps.empty()) {
    return;
  }
  ShowIntentPickerBubbleForApps(
      web_contents, std::move(apps),
      /*show_stay_in_chrome=*/false,
      /*show_remember_selection=*/false,
      base::BindOnce(&OnIntentPickerClosed, web_contents, nullptr, url));
#endif
}

}  // namespace apps
