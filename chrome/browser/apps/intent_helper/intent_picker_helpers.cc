// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/intent_picker_helpers.h"

#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/intent_helper/intent_picker_auto_display_prefs.h"
#include "chrome/browser/apps/intent_helper/intent_picker_constants.h"
#include "chrome/browser/apps/intent_helper/intent_picker_features.h"
#include "chrome/browser/apps/intent_helper/intent_picker_internal.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/feature_engagement/public/tracker.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/favicon_size.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/apps/intent_helper/chromeos_intent_picker_helpers.h"
#include "chrome/browser/apps/intent_helper/metrics/intent_handling_metrics.h"
#elif BUILDFLAG(IS_MAC)
#include "base/task/thread_pool.h"
#include "chrome/browser/apps/intent_helper/mac_intent_picker_helpers.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace apps {

namespace {

bool ShouldConsiderWebContentsForIntentPicker(
    content::WebContents* web_contents) {
  if (!web_app::AreWebAppsUserInstallable(
          Profile::FromBrowserContext(web_contents->GetBrowserContext())) ||
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents) != nullptr) {
    return false;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return false;
  }

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser && (browser->is_type_app() || browser->is_type_app_popup())) {
    return false;
  }
  return true;
}

void AppendAppsForUrlSync(
    content::WebContents* web_contents,
    const GURL& url,
    base::OnceCallback<void(std::vector<IntentPickerAppInfo>)> callback,
    std::vector<IntentPickerAppInfo> apps) {
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

  std::move(callback).Run(std::move(apps));
}

void FindAppsForUrl(
    content::WebContents* web_contents,
    const GURL& url,
    base::OnceCallback<void(std::vector<IntentPickerAppInfo>)> callback) {
  auto append_apps =
      [](base::WeakPtr<content::WebContents> web_contents, int commit_count,
         const GURL& url,
         base::OnceCallback<void(std::vector<IntentPickerAppInfo>)> callback,
         std::vector<IntentPickerAppInfo> apps) {
        if (!web_contents)
          return;
        IntentPickerTabHelper* helper =
            IntentPickerTabHelper::FromWebContents(web_contents.get());
        if (helper->commit_count() != commit_count)
          return;

        AppendAppsForUrlSync(web_contents.get(), url, std::move(callback),
                             std::move(apps));
      };

  IntentPickerTabHelper* helper =
      IntentPickerTabHelper::FromWebContents(web_contents);
  int commit_count = helper->commit_count();

#if BUILDFLAG(IS_MAC)
  // On the Mac, if there is a Universal Link, it goes first. Jump to a worker
  // thread to do this.

  auto get_mac_app = [](const GURL& url) {
    std::vector<IntentPickerAppInfo> apps;
    if (absl::optional<IntentPickerAppInfo> mac_app = FindMacAppForUrl(url))
      apps.push_back(std::move(mac_app.value()));
    return apps;
  };

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING, base::MayBlock()},
      base::BindOnce(get_mac_app, url),
      base::BindOnce(append_apps, web_contents->GetWeakPtr(), commit_count, url,
                     std::move(callback)));
#else
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(append_apps, web_contents->GetWeakPtr(), commit_count, url,
                     std::move(callback), std::vector<IntentPickerAppInfo>()));
#endif  // BUILDFLAG(IS_MAC)
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
    IntentPickerAutoDisplayPrefs::ResetIntentChipCounter(profile, url);
    chrome::FindBrowserWithWebContents(web_contents)->window()
        ->NotifyFeatureEngagementEvent(kIntentChipOpensAppEvent);
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

void OnIntentPickerClosed(base::WeakPtr<content::WebContents> web_contents,
                          const GURL& url,
                          const std::string& launch_name,
                          PickerEntryType entry_type,
                          IntentPickerCloseReason close_reason,
                          bool should_persist) {
  if (!web_contents) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS)
  OnIntentPickerClosedChromeOs(web_contents, url, launch_name, entry_type,
                               close_reason, should_persist);
#else
  const bool should_launch_app =
      close_reason == apps::IntentPickerCloseReason::OPEN_APP;
  if (should_launch_app) {
    LaunchAppFromIntentPicker(web_contents.get(), url, launch_name, entry_type);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void OnAppIconsLoaded(content::WebContents* web_contents,
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
      base::BindOnce(&OnIntentPickerClosed, web_contents->GetWeakPtr(), url));
}

void GetAppsForIntentPicker(
    content::WebContents* web_contents,
    base::OnceCallback<void(std::vector<IntentPickerAppInfo>)> callback) {
  if (!ShouldConsiderWebContentsForIntentPicker(web_contents)) {
    std::move(callback).Run({});
    return;
  }

  FindAppsForUrl(web_contents, web_contents->GetLastCommittedURL(),
                 std::move(callback));
}

void ShowIntentPickerOrLaunchAppImpl(content::WebContents* web_contents,
                                     const GURL& url,
                                     std::vector<IntentPickerAppInfo> apps) {
  if (apps.empty())
    return;

#if BUILDFLAG(IS_CHROMEOS)
  apps::IntentHandlingMetrics::RecordIntentPickerIconEvent(
      apps::IntentHandlingMetrics::IntentPickerIconEvent::kIconClicked);
#endif

  if (apps.size() == 1) {
    // If there is only a single available app, immediately launch it if either:
    // - LinkCapturingInfoBarEnabled() is enabled, or
    // - LinkCapturingUiUpdateEnabled() is enabled and the app is preferred for
    // this link.
    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile);

    bool should_launch_for_preferred_app =
        apps::features::LinkCapturingUiUpdateEnabled() &&
        proxy->PreferredAppsList().FindPreferredAppForUrl(url) ==
            apps[0].launch_name;

    if (apps::features::LinkCapturingInfoBarEnabled() ||
        should_launch_for_preferred_app) {
      LaunchAppFromIntentPicker(web_contents, url, apps[0].launch_name,
                                apps[0].type);
      return;
    }
  }

  IntentPickerTabHelper::LoadAppIcons(
      web_contents, std::move(apps),
      base::BindOnce(&OnAppIconsLoaded, web_contents, url));
}

}  // namespace

void MaybeShowIntentPicker(content::WebContents* web_contents) {
  IntentPickerTabHelper* helper =
      IntentPickerTabHelper::FromWebContents(web_contents);
  int commit_count = helper->commit_count();

  auto task = [](base::WeakPtr<content::WebContents> web_contents,
                 IntentPickerTabHelper* helper, int commit_count,
                 std::vector<IntentPickerAppInfo> apps) {
    if (!web_contents)
      return;
    if (helper->commit_count() != commit_count)
      return;

    helper->ShowIconForApps(apps);
  };

  GetAppsForIntentPicker(
      web_contents,
      base::BindOnce(task, web_contents->GetWeakPtr(), helper, commit_count));
}

void ShowIntentPickerOrLaunchApp(content::WebContents* web_contents,
                                 const GURL& url) {
  FindAppsForUrl(
      web_contents, url,
      base::BindOnce(&ShowIntentPickerOrLaunchAppImpl, web_contents, url));
}

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

}  // namespace apps
