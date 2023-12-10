// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/enterprise_remote_apps/enterprise_remote_apps_api.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/apps/platform_apps/api/enterprise_remote_apps.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"
#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros_factory.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#endif

namespace chrome_apps::api {

namespace {

chromeos::remote_apps::mojom::RemoteApps* GetEnterpriseRemoteAppsApi(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This also validates that the Remote Apps Mojo interface is available in
  // Ash via the factory.
  return chromeos::RemoteAppsProxyLacrosFactory::GetForBrowserContext(profile);
#else
  ash::RemoteAppsManager* remote_apps_manager =
      ash::RemoteAppsManagerFactory::GetForProfile(profile);
  if (remote_apps_manager == nullptr)
    return nullptr;

  return &remote_apps_manager->GetRemoteAppsImpl();
#endif
}

}  // namespace

using enterprise_remote_apps::RemoteAppsPosition;

EnterpriseRemoteAppsAddFolderFunction::EnterpriseRemoteAppsAddFolderFunction() =
    default;

EnterpriseRemoteAppsAddFolderFunction::
    ~EnterpriseRemoteAppsAddFolderFunction() = default;

ExtensionFunction::ResponseAction EnterpriseRemoteAppsAddFolderFunction::Run() {
  auto parameters =
      api::enterprise_remote_apps::AddFolder::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  chromeos::remote_apps::mojom::RemoteApps* remote_apps_api =
      GetEnterpriseRemoteAppsApi(browser_context());
  if (remote_apps_api == nullptr)
    return RespondNow(Error("Remote apps not supported in this session"));

  const auto& options = parameters->options;
  bool add_to_front = options.add_to_front ? *options.add_to_front : false;

  auto callback =
      base::BindOnce(&EnterpriseRemoteAppsAddFolderFunction::OnResult, this);
  remote_apps_api->AddFolder(options.name, add_to_front, std::move(callback));

  // `did_respond()` needed here as the `AddFolder()` can be sync or async.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EnterpriseRemoteAppsAddFolderFunction::OnResult(
    chromeos::remote_apps::mojom::AddFolderResultPtr result) {
  if (result->is_error()) {
    Respond(Error(result->get_error()));
    return;
  }

  Respond(WithArguments(result->get_folder_id()));
}

EnterpriseRemoteAppsAddAppFunction::EnterpriseRemoteAppsAddAppFunction() =
    default;

EnterpriseRemoteAppsAddAppFunction::~EnterpriseRemoteAppsAddAppFunction() =
    default;

ExtensionFunction::ResponseAction EnterpriseRemoteAppsAddAppFunction::Run() {
  auto parameters = api::enterprise_remote_apps::AddApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  chromeos::remote_apps::mojom::RemoteApps* remote_apps_api =
      GetEnterpriseRemoteAppsApi(browser_context());
  if (remote_apps_api == nullptr)
    return RespondNow(Error("Remote apps not supported in this session"));

  const auto& options = parameters->options;

  // Checks that `icon_url` is a valid URL. If it is not valid, or it was not
  // provided at all, we send an empty URL and the icon is replaced with the
  // placeholder icon.
  GURL icon_url;
  if (options.icon_url) {
    icon_url = GURL(*options.icon_url);
    if (!icon_url.is_valid())
      icon_url = GURL();
  }

  bool add_to_front = options.add_to_front ? *options.add_to_front : false;

  std::string folder_id;
  if (options.folder_id)
    folder_id = *options.folder_id;

  auto callback =
      base::BindOnce(&EnterpriseRemoteAppsAddAppFunction::OnResult, this);
  remote_apps_api->AddApp(extension_id(), options.name, folder_id, icon_url,
                          add_to_front, std::move(callback));

  // `did_respond()` needed here as the `AddApp()` can be sync or async.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EnterpriseRemoteAppsAddAppFunction::OnResult(
    chromeos::remote_apps::mojom::AddAppResultPtr result) {
  if (result->is_error()) {
    Respond(Error(result->get_error()));
    return;
  }

  Respond(WithArguments(result->get_app_id()));
}

EnterpriseRemoteAppsDeleteAppFunction::EnterpriseRemoteAppsDeleteAppFunction() =
    default;

EnterpriseRemoteAppsDeleteAppFunction::
    ~EnterpriseRemoteAppsDeleteAppFunction() = default;

ExtensionFunction::ResponseAction EnterpriseRemoteAppsDeleteAppFunction::Run() {
  auto parameters =
      api::enterprise_remote_apps::DeleteApp::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  chromeos::remote_apps::mojom::RemoteApps* remote_apps_api =
      GetEnterpriseRemoteAppsApi(browser_context());
  if (remote_apps_api == nullptr)
    return RespondNow(Error("Remote apps not supported in this session"));

  auto callback =
      base::BindOnce(&EnterpriseRemoteAppsDeleteAppFunction::OnResult, this);
  remote_apps_api->DeleteApp(parameters->app_id, std::move(callback));

  // `did_respond()` needed here as the `DeleteApp()` can be sync or async.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EnterpriseRemoteAppsDeleteAppFunction::OnResult(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  Respond(NoArguments());
}

EnterpriseRemoteAppsSortLauncherFunction::
    EnterpriseRemoteAppsSortLauncherFunction() = default;

EnterpriseRemoteAppsSortLauncherFunction::
    ~EnterpriseRemoteAppsSortLauncherFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseRemoteAppsSortLauncherFunction::Run() {
  auto parameters =
      api::enterprise_remote_apps::SortLauncher::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  chromeos::remote_apps::mojom::RemoteApps* remote_apps_api =
      GetEnterpriseRemoteAppsApi(browser_context());
  if (remote_apps_api == nullptr)
    return RespondNow(Error("Remote apps not supported in this session"));

  switch (parameters->options.position) {
    case RemoteAppsPosition::kRemoteAppsFirst: {
      auto callback = base::BindOnce(
          &EnterpriseRemoteAppsSortLauncherFunction::OnResult, this);
      remote_apps_api->SortLauncherWithRemoteAppsFirst(std::move(callback));
      break;
    }
    default: {
      return RespondNow(Error("Remote apps sort position not valid."));
    }
  }

  // `did_respond()` needed here as the `SortLauncher()` can be sync or async.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EnterpriseRemoteAppsSortLauncherFunction::OnResult(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  Respond(NoArguments());
}

EnterpriseRemoteAppsSetPinnedAppsFunction::
    EnterpriseRemoteAppsSetPinnedAppsFunction() = default;

EnterpriseRemoteAppsSetPinnedAppsFunction::
    ~EnterpriseRemoteAppsSetPinnedAppsFunction() = default;

ExtensionFunction::ResponseAction
EnterpriseRemoteAppsSetPinnedAppsFunction::Run() {
  auto parameters =
      api::enterprise_remote_apps::SetPinnedApps::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  chromeos::remote_apps::mojom::RemoteApps* remote_apps_api =
      GetEnterpriseRemoteAppsApi(browser_context());
  if (remote_apps_api == nullptr) {
    return RespondNow(Error("Remote apps not supported in this session"));
  }

  auto callback = base::BindOnce(
      &EnterpriseRemoteAppsSetPinnedAppsFunction::OnResult, this);
  remote_apps_api->SetPinnedApps(parameters->app_ids, std::move(callback));

  // `did_respond()` needed here as the `SetPinnedApps()` can be sync or async.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void EnterpriseRemoteAppsSetPinnedAppsFunction::OnResult(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  Respond(NoArguments());
}

}  // namespace chrome_apps::api
