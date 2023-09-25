// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"

#include <string>

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/apps_navigation_types.h"
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/apps/intent_helper/intent_chip_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/share/share_attempt.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"
#endif

namespace apps {
namespace {

std::vector<apps::IntentPickerAppInfo> CombinePossibleMacAppWithOtherApps(
    std::vector<apps::IntentPickerAppInfo> apps,
    absl::optional<apps::IntentPickerAppInfo> mac_app) {
  if (mac_app) {
    apps.emplace_back(std::move(mac_app.value()));
  }
  return apps;
}

PickerEntryType GetPickerEntryType(AppType app_type) {
  PickerEntryType picker_entry_type = PickerEntryType::kUnknown;
  switch (app_type) {
    case AppType::kUnknown:
    case AppType::kBuiltIn:
    case AppType::kCrostini:
    case AppType::kPluginVm:
    case AppType::kChromeApp:
    case AppType::kExtension:
    case AppType::kStandaloneBrowser:
    case AppType::kStandaloneBrowserChromeApp:
    case AppType::kRemote:
    case AppType::kBorealis:
    case AppType::kBruschetta:
    case AppType::kStandaloneBrowserExtension:
      break;
    case AppType::kArc:
      picker_entry_type = PickerEntryType::kArc;
      break;
    case AppType::kWeb:
    case AppType::kSystemWeb:
      picker_entry_type = PickerEntryType::kWeb;
      break;
    case AppType::kMacOs:
      picker_entry_type = PickerEntryType::kMacOs;
      break;
  }
  return picker_entry_type;
}

}  // namespace

bool IntentPickerPwaPersistenceEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  return true;
#else
  return false;
#endif
}

int GetIntentPickerBubbleIconSize() {
  constexpr int kIntentPickerUiUpdateIconSize = 40;

  return features::LinkCapturingUiUpdateEnabled()
             ? kIntentPickerUiUpdateIconSize
             : gfx::kFaviconSize;
}

void FindAllAppsForUrl(
    Profile* profile,
    const GURL& url,
    base::OnceCallback<void(std::vector<apps::IntentPickerAppInfo>)> callback) {
  CHECK(profile);
  std::vector<apps::IntentPickerAppInfo> apps;

  AppServiceProxy* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);

  // TODO(dmurph): Use the WebAppProvider instead of the AppService on non-CrOS
  // platforms.
  std::vector<std::string> app_ids =
      proxy->GetAppIdsForUrl(url, /*exclude_browsers=*/true);

  for (const std::string& app_id : app_ids) {
    proxy->AppRegistryCache().ForOneApp(
        app_id, [&apps](const apps::AppUpdate& update) {
          apps.emplace_back(GetPickerEntryType(update.AppType()),
                            ui::ImageModel(), update.AppId(), update.Name());
        });
  }
  // Reverse to keep old behavior of ordering (even though it was arbitrary, it
  // was at least deterministic).
  std::reverse(apps.begin(), apps.end());

#if BUILDFLAG(IS_MAC)
  // On the Mac, if there is a Universal Link, jump to a worker thread to do
  // this.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(&FindMacAppForUrl, url),
      base::BindOnce(&CombinePossibleMacAppWithOtherApps, std::move(apps))
          .Then(std::move(callback)));
#else
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(apps)));
#endif  // BUILDFLAG(IS_MAC)
}

void LaunchAppFromIntentPicker(content::WebContents* web_contents,
                               const GURL& url,
                               const std::string& launch_name,
                               apps::PickerEntryType app_type) {
#if BUILDFLAG(IS_CHROMEOS)
  LaunchAppFromIntentPickerChromeOs(web_contents, url, launch_name, app_type);
#else

  if (base::FeatureList::IsEnabled(apps::features::kLinkCapturingUiUpdate)) {
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    IntentChipDisplayPrefs::ResetIntentChipCounter(profile, url);
  }

  switch (app_type) {
    case apps::PickerEntryType::kWeb:
      web_app::ReparentWebContentsIntoAppBrowser(web_contents, launch_name);
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
