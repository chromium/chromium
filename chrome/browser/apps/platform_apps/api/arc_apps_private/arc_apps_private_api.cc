// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/arc_apps_private/arc_apps_private_api.h"

#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/apps/platform_apps/api/arc_apps_private.h"
#include "ui/events/event_constants.h"

namespace chrome_apps {
namespace api {

// static
extensions::BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>*
ArcAppsPrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<
      extensions::BrowserContextKeyedAPIFactory<ArcAppsPrivateAPI>>
      instance;
  return instance.get();
}

ArcAppsPrivateAPI::ArcAppsPrivateAPI(content::BrowserContext* context)
    : context_(context) {
  extensions::EventRouter::Get(context_)->RegisterObserver(
      this, api::arc_apps_private::OnInstalled::kEventName);
}

ArcAppsPrivateAPI::~ArcAppsPrivateAPI() = default;

void ArcAppsPrivateAPI::Shutdown() {
  extensions::EventRouter::Get(context_)->UnregisterObserver(this);
  scoped_prefs_observation_.Reset();
}

void ArcAppsPrivateAPI::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  DCHECK_EQ(details.event_name, api::arc_apps_private::OnInstalled::kEventName);
  auto* prefs = ArcAppListPrefs::Get(Profile::FromBrowserContext(context_));
  if (prefs && !scoped_prefs_observation_.IsObservingSource(prefs))
    scoped_prefs_observation_.Observe(prefs);
}

void ArcAppsPrivateAPI::OnListenerRemoved(
    const extensions::EventListenerInfo& details) {
  if (!extensions::EventRouter::Get(context_)->HasEventListener(
          api::arc_apps_private::OnInstalled::kEventName)) {
    scoped_prefs_observation_.Reset();
  }
}

void ArcAppsPrivateAPI::OnAppRegistered(
    const std::string& app_id,
    const ArcAppListPrefs::AppInfo& app_info) {
  if (!app_info.launchable)
    return;
  api::arc_apps_private::AppInfo app_info_result;
  app_info_result.package_name = app_info.package_name;
  auto event = std::make_unique<extensions::Event>(
      extensions::events::ARC_APPS_PRIVATE_ON_INSTALLED,
      api::arc_apps_private::OnInstalled::kEventName,
      api::arc_apps_private::OnInstalled::Create(app_info_result), context_);
  extensions::EventRouter::Get(context_)->BroadcastEvent(std::move(event));
}

ArcAppsPrivateGetLaunchableAppsFunction::
    ArcAppsPrivateGetLaunchableAppsFunction() = default;

ArcAppsPrivateGetLaunchableAppsFunction::
    ~ArcAppsPrivateGetLaunchableAppsFunction() = default;

ExtensionFunction::ResponseAction
ArcAppsPrivateGetLaunchableAppsFunction::Run() {
  auto* prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs)
    return RespondNow(Error("Not available"));

  std::vector<api::arc_apps_private::AppInfo> result;
  const std::vector<std::string> app_ids = prefs->GetAppIds();
  for (const auto& app_id : app_ids) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info && app_info->launchable) {
      api::arc_apps_private::AppInfo app_info_result;
      app_info_result.package_name = app_info->package_name;
      result.push_back(std::move(app_info_result));
    }
  }
  return RespondNow(ArgumentList(
      api::arc_apps_private::GetLaunchableApps::Results::Create(result)));
}

ArcAppsPrivateLaunchAppFunction::ArcAppsPrivateLaunchAppFunction() = default;

ArcAppsPrivateLaunchAppFunction::~ArcAppsPrivateLaunchAppFunction() = default;

ExtensionFunction::ResponseAction ArcAppsPrivateLaunchAppFunction::Run() {
  std::optional<api::arc_apps_private::LaunchApp::Params> params(
      api::arc_apps_private::LaunchApp::Params::Create(args()));
  EXTENSION_FUNCTION_VALIDATE(params.has_value());
  ArcAppListPrefs* prefs =
      ArcAppListPrefs::Get(Profile::FromBrowserContext(browser_context()));
  if (!prefs)
    return RespondNow(Error("Not available"));

  const std::string app_id = prefs->GetAppIdByPackageName(params->package_name);
  if (app_id.empty())
    return RespondNow(Error("App not found"));

  if (!arc::LaunchApp(
          browser_context(), app_id, ui::EF_NONE,
          arc::UserInteractionType::APP_STARTED_FROM_EXTENSION_API)) {
    return RespondNow(Error("Launch failed"));
  }

  return RespondNow(NoArguments());
}

}  // namespace api
}  // namespace chrome_apps
