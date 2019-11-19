// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/intent_helper/arc_intent_picker_app_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/chromeos/apps/intent_helper/chromeos_apps_navigation_throttle.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace arc {

namespace {

// Searches for a preferred app in |app_candidates| and returns its index. If
// not found, returns |app_candidates.size()|.
size_t FindPreferredApp(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates,
    const GURL& url_for_logging) {
  for (size_t i = 0; i < app_candidates.size(); ++i) {
    if (!app_candidates[i]->is_preferred)
      continue;
    if (ArcIntentHelperBridge::IsIntentHelperPackage(
            app_candidates[i]->package_name)) {
      // If Chrome browser was selected as the preferred app, we shouldn't
      // create a throttle.
      DVLOG(1)
          << "Chrome browser is selected as the preferred app for this URL: "
          << url_for_logging;
    }
    return i;
  }
  return app_candidates.size();  // not found
}

}  // namespace

// static
void ArcIntentPickerAppFetcher::GetArcAppsForPicker(
    content::WebContents* web_contents,
    const GURL& url,
    apps::GetAppsCallback callback) {
  arc::ArcServiceManager* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    DVLOG(1) << "Cannot get an instance of ArcServiceManager";
    std::move(callback).Run({});
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance) {
    DVLOG(1) << "Cannot get access to RequestUrlHandlerList";
    std::move(callback).Run({});
    return;
  }

  // |app_fetcher| will delete itself when it is finished.
  ArcIntentPickerAppFetcher* app_fetcher =
      new ArcIntentPickerAppFetcher(web_contents);
  app_fetcher->GetArcAppsForPicker(instance, url, std::move(callback));
}

// static
bool ArcIntentPickerAppFetcher::WillGetArcAppsForNavigation(
    content::NavigationHandle* handle,
    apps::AppsNavigationCallback callback,
    bool should_launch_preferred_app) {
  ArcServiceManager* arc_service_manager = ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;

  content::WebContents* web_contents = handle->GetWebContents();
  auto* intent_helper_bridge = ArcIntentHelperBridge::GetForBrowserContext(
      web_contents->GetBrowserContext());
  if (!intent_helper_bridge)
    return false;

  const GURL& url = handle->GetURL();
  if (intent_helper_bridge->ShouldChromeHandleUrl(url)) {
    // Allow navigation to proceed if there isn't an android app that handles
    // the given URL.
    return false;
  }

  mojom::IntentHelperInstance* instance = ARC_GET_INSTANCE_FOR_METHOD(
      arc_service_manager->arc_bridge_service()->intent_helper(),
      RequestUrlHandlerList);
  if (!instance)
    return false;

  // |app_fetcher| will delete itself when it is finished.
  ArcIntentPickerAppFetcher* app_fetcher =
      new ArcIntentPickerAppFetcher(web_contents);

  // Return true to defer the navigation until we asynchronously hear back from
  // ARC whether a preferred app should be launched. This makes it safe to bind
  // |handle| as a raw pointer argument. We will either resume or cancel the
  // navigation as soon as the callback is run. If the WebContents is destroyed
  // prior to this asynchronous method finishing, it is safe to not run
  // |callback| since it will not matter what we do with the deferred navigation
  // for a now-closed tab. If |should_launch_preferred_app| flag is set to be
  // true, preferred app (if exists) will be automatically launched.
  app_fetcher->GetArcAppsForNavigation(instance, url, std::move(callback),
                                       should_launch_preferred_app);
  return true;
}

// static
bool ArcIntentPickerAppFetcher::MaybeLaunchOrPersistArcApp(
    const GURL& url,
    const std::string& package_name,
    bool should_launch,
    bool should_persist) {
  auto* arc_service_manager = arc::ArcServiceManager::Get();
  arc::mojom::IntentHelperInstance* instance = nullptr;
  if (arc_service_manager) {
    instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->intent_helper(), HandleUrl);
  }

  // With no instance, or if we neither want to launch an ARC app or persist the
  // preference to the container, return early.
  if (!instance || !(should_launch || should_persist))
    return false;

  if (should_persist) {
    DCHECK(arc_service_manager);
    if (ARC_GET_INSTANCE_FOR_METHOD(
            arc_service_manager->arc_bridge_service()->intent_helper(),
            AddPreferredPackage)) {
      instance->AddPreferredPackage(package_name);
    }
  }

  if (should_launch) {
    instance->HandleUrl(url.spec(), package_name);
    return true;
  }

  return false;
}

// static
size_t ArcIntentPickerAppFetcher::GetAppIndex(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates,
    const std::string& selected_app_package) {
  for (size_t i = 0; i < app_candidates.size(); ++i) {
    if (app_candidates[i]->package_name == selected_app_package)
      return i;
  }
  return app_candidates.size();
}

// static
bool ArcIntentPickerAppFetcher::IsAppAvailable(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  return app_candidates.size() > 1 ||
         (app_candidates.size() == 1 &&
          !ArcIntentHelperBridge::IsIntentHelperPackage(
              app_candidates[0]->package_name));
}

// static
bool ArcIntentPickerAppFetcher::IsAppAvailableForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  return IsAppAvailable(app_candidates);
}

// static
size_t ArcIntentPickerAppFetcher::FindPreferredAppForTesting(
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  return FindPreferredApp(app_candidates, /*url_for_logging=*/GURL());
}

ArcIntentPickerAppFetcher::~ArcIntentPickerAppFetcher() = default;

ArcIntentPickerAppFetcher::ArcIntentPickerAppFetcher(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

void ArcIntentPickerAppFetcher::GetArcAppsForNavigation(
    mojom::IntentHelperInstance* instance,
    const GURL& url,
    apps::AppsNavigationCallback callback,
    bool should_launch_preferred_app) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  instance->RequestUrlHandlerList(
      url.spec(),
      base::BindOnce(
          &ArcIntentPickerAppFetcher::OnAppCandidatesReceivedForNavigation,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(callback),
          should_launch_preferred_app));
}

void ArcIntentPickerAppFetcher::GetArcAppsForPicker(
    mojom::IntentHelperInstance* instance,
    const GURL& url,
    apps::GetAppsCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  instance->RequestUrlHandlerList(
      url.spec(),
      base::BindOnce(
          &ArcIntentPickerAppFetcher::OnAppCandidatesReceivedForPicker,
          weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)));
}

void ArcIntentPickerAppFetcher::OnAppCandidatesReceivedForNavigation(
    const GURL& url,
    apps::AppsNavigationCallback callback,
    bool should_launch_preferred_app,
    std::vector<mojom::IntentHandlerInfoPtr> app_candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ArcIntentPickerAppFetcher> deleter(this);
  if (!IsAppAvailable(app_candidates)) {
    // This scenario shouldn't be accessed as ArcIntentPickerAppFetcher is
    // created iff there are ARC apps which can actually handle the given URL.
    DVLOG(1) << "There are no app candidates for this URL: " << url;
    chromeos::ChromeOsAppsNavigationThrottle::RecordUma(
        /*selected_app_package=*/std::string(), apps::PickerEntryType::kUnknown,
        apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER,
        apps::Source::kHttpOrHttps,
        /*should_persist=*/false);
    std::move(callback).Run(apps::AppsNavigationAction::RESUME, {});
    return;
  }

  if (should_launch_preferred_app) {
    // If one of the apps is marked as preferred, launch it immediately.
    apps::PreferredPlatform pref_platform =
        DidLaunchPreferredArcApp(url, app_candidates);

    switch (pref_platform) {
      case apps::PreferredPlatform::ARC:
        std::move(callback).Run(apps::AppsNavigationAction::CANCEL, {});
        return;
      case apps::PreferredPlatform::NATIVE_CHROME:
        std::move(callback).Run(apps::AppsNavigationAction::RESUME, {});
        return;
      case apps::PreferredPlatform::PWA:
        NOTREACHED();
        break;
      case apps::PreferredPlatform::NONE:
        break;  // Do nothing.
    }
  }
  // We are always going to resume navigation at this point, and possibly show
  // the intent picker bubble to prompt the user to choose if they would like to
  // use an ARC app to open the URL.
  deleter.release();
  GetArcAppIcons(
      url, std::move(app_candidates),
      base::BindOnce(std::move(callback), apps::AppsNavigationAction::RESUME));
}

void ArcIntentPickerAppFetcher::OnAppCandidatesReceivedForPicker(
    const GURL& url,
    apps::GetAppsCallback callback,
    std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ArcIntentPickerAppFetcher> deleter(this);
  if (!IsAppAvailable(app_candidates)) {
    DVLOG(1) << "There are no app candidates for this URL";
    std::move(callback).Run({});
    return;
  }

  deleter.release();
  GetArcAppIcons(url, std::move(app_candidates), std::move(callback));
}

apps::PreferredPlatform ArcIntentPickerAppFetcher::DidLaunchPreferredArcApp(
    const GURL& url,
    const std::vector<mojom::IntentHandlerInfoPtr>& app_candidates) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  apps::PreferredPlatform preferred_platform = apps::PreferredPlatform::NONE;
  apps::PickerEntryType entry_type = apps::PickerEntryType::kUnknown;
  const size_t index = FindPreferredApp(app_candidates, url);

  if (index != app_candidates.size()) {
    auto close_reason = apps::IntentPickerCloseReason::PREFERRED_APP_FOUND;
    const std::string& package_name = app_candidates[index]->package_name;

    // Make sure that the instance at least supports HandleUrl.
    auto* arc_service_manager = ArcServiceManager::Get();
    mojom::IntentHelperInstance* instance = nullptr;
    if (arc_service_manager) {
      instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
    }

    if (!instance) {
      close_reason = apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER;
    } else if (ArcIntentHelperBridge::IsIntentHelperPackage(package_name)) {
      IntentPickerTabHelper::SetShouldShowIcon(web_contents(), true);
      preferred_platform = apps::PreferredPlatform::NATIVE_CHROME;
    } else {
      instance->HandleUrl(url.spec(), package_name);
      preferred_platform = apps::PreferredPlatform::ARC;
      entry_type = apps::PickerEntryType::kArc;
    }
    chromeos::ChromeOsAppsNavigationThrottle::RecordUma(
        package_name, entry_type, close_reason, apps::Source::kHttpOrHttps,
        /*should_persist=*/false);
  }

  return preferred_platform;
}

void ArcIntentPickerAppFetcher::GetArcAppIcons(
    const GURL& url,
    std::vector<mojom::IntentHandlerInfoPtr> app_candidates,
    apps::GetAppsCallback callback) {
  std::unique_ptr<ArcIntentPickerAppFetcher> deleter(this);
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto* intent_helper_bridge = ArcIntentHelperBridge::GetForBrowserContext(
      web_contents()->GetBrowserContext());
  if (!intent_helper_bridge) {
    LOG(ERROR) << "Cannot get an instance of ArcIntentHelperBridge";
    chromeos::ChromeOsAppsNavigationThrottle::RecordUma(
        /*selected_app_package=*/std::string(), apps::PickerEntryType::kUnknown,
        apps::IntentPickerCloseReason::ERROR_BEFORE_PICKER,
        apps::Source::kHttpOrHttps, /*should_persist=*/false);
    std::move(callback).Run({});
    return;
  }
  std::vector<ArcIntentHelperBridge::ActivityName> activities;
  for (const auto& candidate : app_candidates)
    activities.emplace_back(candidate->package_name, candidate->activity_name);

  deleter.release();
  intent_helper_bridge->GetActivityIcons(
      activities,
      base::BindOnce(&ArcIntentPickerAppFetcher::OnAppIconsReceived,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     std::move(app_candidates), std::move(callback)));
}

void ArcIntentPickerAppFetcher::OnAppIconsReceived(
    const GURL& url,
    std::vector<arc::mojom::IntentHandlerInfoPtr> app_candidates,
    apps::GetAppsCallback callback,
    std::unique_ptr<arc::ArcIntentHelperBridge::ActivityToIconsMap> icons) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<ArcIntentPickerAppFetcher> deleter(this);
  std::vector<apps::IntentPickerAppInfo> app_info;

  for (const auto& candidate : app_candidates) {
    gfx::Image icon;
    const arc::ArcIntentHelperBridge::ActivityName activity(
        candidate->package_name, candidate->activity_name);
    const auto it = icons->find(activity);

    app_info.emplace_back(apps::PickerEntryType::kArc,
                          it != icons->end() ? it->second.icon16 : gfx::Image(),
                          candidate->package_name, candidate->name);
  }

  // After running the callback, |this| is always deleted by |deleter| going out
  // of scope.
  std::move(callback).Run(std::move(app_info));
}

void ArcIntentPickerAppFetcher::WebContentsDestroyed() {
  delete this;
}

}  // namespace arc
