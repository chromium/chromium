// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/remote_apps/remote_apps_proxy_lacros.h"

#include <optional>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/apps/platform_apps/api/enterprise_remote_apps.h"
#include "chromeos/lacros/lacros_service.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace chromeos {

// static
std::unique_ptr<RemoteAppsProxyLacros> RemoteAppsProxyLacros::CreateForTesting(
    Profile* profile,
    mojo::Remote<remote_apps::mojom::RemoteAppsLacrosBridge>&
        remote_apps_lacros_bridge) {
  // Using `new` to access non-public constructor.
  return base::WrapUnique(
      new RemoteAppsProxyLacros(profile, remote_apps_lacros_bridge));
}

// Availability of the `RemoteAppsFactory` remote is checked in
// `RemoteAppsProxyLacrosFactory`.
RemoteAppsProxyLacros::RemoteAppsProxyLacros(Profile* profile)
    : RemoteAppsProxyLacros(
          profile,
          LacrosService::Get()
              ->GetRemote<remote_apps::mojom::RemoteAppsLacrosBridge>()) {}

RemoteAppsProxyLacros::RemoteAppsProxyLacros(
    Profile* profile,
    mojo::Remote<remote_apps::mojom::RemoteAppsLacrosBridge>&
        remote_apps_lacros_bridge)
    : event_router_(extensions::EventRouter::Get(profile)) {
  remote_apps_lacros_bridge->BindRemoteAppsAndAppLaunchObserverForLacros(
      ash_remote_apps_remote_.BindNewPipeAndPassReceiver(),
      ash_observer_receiver_.BindNewPipeAndPassRemoteWithVersion());

  proxy_app_launch_observers_.set_disconnect_handler(base::BindRepeating(
      &RemoteAppsProxyLacros::DisconnectHandler, base::Unretained(this)));
}

RemoteAppsProxyLacros::~RemoteAppsProxyLacros() = default;

void RemoteAppsProxyLacros::BindFactoryInterface(
    mojo::PendingReceiver<remote_apps::mojom::RemoteAppsFactory>
        pending_remote_apps_factory) {
  proxy_factory_receivers_.Add(this, std::move(pending_remote_apps_factory));
}

void RemoteAppsProxyLacros::BindRemoteAppsAndAppLaunchObserver(
    const std::string& source_id,
    mojo::PendingReceiver<remote_apps::mojom::RemoteApps> pending_remote_apps,
    mojo::PendingRemote<remote_apps::mojom::RemoteAppLaunchObserver>
        pending_observer) {
  proxy_remote_apps_receivers_.Add(this, std::move(pending_remote_apps));

  const mojo::RemoteSetElementId& remote_id = proxy_app_launch_observers_.Add(
      mojo::Remote<remote_apps::mojom::RemoteAppLaunchObserver>(
          std::move(pending_observer)));
  source_id_to_remote_id_map_.insert(
      std::pair<std::string, mojo::RemoteSetElementId>(source_id, remote_id));
}

void RemoteAppsProxyLacros::AddFolder(const std::string& name,
                                      bool add_to_front,
                                      AddFolderCallback callback) {
  if (!ash_remote_apps_remote_.is_bound() ||
      !ash_remote_apps_remote_.is_connected()) {
    std::move(callback).Run(remote_apps::mojom::AddFolderResult::NewError(
        kErrorNoAshRemoteConnected));
    return;
  }

  ash_remote_apps_remote_->AddFolder(name, add_to_front, std::move(callback));
}

void RemoteAppsProxyLacros::AddApp(const std::string& source_id,
                                   const std::string& name,
                                   const std::string& folder_id,
                                   const GURL& icon_url,
                                   bool add_to_front,
                                   AddAppCallback callback) {
  if (!ash_remote_apps_remote_.is_bound() ||
      !ash_remote_apps_remote_.is_connected()) {
    std::move(callback).Run(
        remote_apps::mojom::AddAppResult::NewError(kErrorNoAshRemoteConnected));
    return;
  }

  ash_remote_apps_remote_->AddApp(source_id, name, folder_id, icon_url,
                                  add_to_front, std::move(callback));
}

void RemoteAppsProxyLacros::DeleteApp(const std::string& app_id,
                                      DeleteAppCallback callback) {
  if (!ash_remote_apps_remote_.is_bound() ||
      !ash_remote_apps_remote_.is_connected()) {
    std::move(callback).Run(kErrorNoAshRemoteConnected);
    return;
  }

  ash_remote_apps_remote_->DeleteApp(app_id, std::move(callback));
}

void RemoteAppsProxyLacros::SortLauncherWithRemoteAppsFirst(
    SortLauncherWithRemoteAppsFirstCallback callback) {
  if (!ash_remote_apps_remote_.is_bound() ||
      !ash_remote_apps_remote_.is_connected()) {
    std::move(callback).Run(kErrorNoAshRemoteConnected);
    return;
  }

  ash_remote_apps_remote_->SortLauncherWithRemoteAppsFirst(std::move(callback));
}

void RemoteAppsProxyLacros::SetPinnedApps(
    const std::vector<std::string>& app_ids,
    SetPinnedAppsCallback callback) {
  if (!ash_remote_apps_remote_.is_bound() ||
      !ash_remote_apps_remote_.is_connected()) {
    std::move(callback).Run(kErrorNoAshRemoteConnected);
    return;
  }
  if (is_ash_remote_apps_remote_version_known_) {
    SetPinnedAppsImpl(app_ids, std::move(callback),
                      ash_remote_apps_remote_.version());
  } else {
    ash_remote_apps_remote_.QueryVersion(
        base::BindOnce(&RemoteAppsProxyLacros::OnVersionForAppPinningReady,
                       base::Unretained(this), app_ids, std::move(callback)));
  }
}

void RemoteAppsProxyLacros::OnRemoteAppLaunched(const std::string& app_id,
                                                const std::string& source_id) {
  std::unique_ptr<extensions::Event> event = std::make_unique<
      extensions::Event>(
      extensions::events::ENTERPRISE_REMOTE_APPS_ON_REMOTE_APP_LAUNCHED,
      chrome_apps::api::enterprise_remote_apps::OnRemoteAppLaunched::kEventName,
      chrome_apps::api::enterprise_remote_apps::OnRemoteAppLaunched::Create(
          app_id));

  event_router_->DispatchEventToExtension(source_id, std::move(event));

  auto it = source_id_to_remote_id_map_.find(source_id);
  if (it == source_id_to_remote_id_map_.end())
    return;

  remote_apps::mojom::RemoteAppLaunchObserver* observer =
      proxy_app_launch_observers_.Get(it->second);
  if (!observer)
    return;

  observer->OnRemoteAppLaunched(app_id, source_id);
}

void RemoteAppsProxyLacros::DisconnectHandler(mojo::RemoteSetElementId id) {
  const auto& it = base::ranges::find(source_id_to_remote_id_map_, id,
                                      &RemoteIds::value_type::second);

  if (it == source_id_to_remote_id_map_.end())
    return;

  source_id_to_remote_id_map_.erase(it);
}

void RemoteAppsProxyLacros::OnVersionForAppPinningReady(
    const std::vector<std::string>& app_ids,
    SetPinnedAppsCallback callback,
    uint32_t interface_version) {
  is_ash_remote_apps_remote_version_known_ = true;
  SetPinnedAppsImpl(app_ids, std::move(callback), interface_version);
}

void RemoteAppsProxyLacros::SetPinnedAppsImpl(
    const std::vector<std::string>& app_ids,
    SetPinnedAppsCallback callback,
    uint32_t interface_version) {
  if (interface_version < remote_apps::mojom::RemoteApps::MethodMinVersions::
                              kSetPinnedAppsMinVersion) {
    std::move(callback).Run(kErrorSetPinnedAppsNotAvailable);
    return;
  }

  ash_remote_apps_remote_->SetPinnedApps(app_ids, std::move(callback));
}

uint32_t RemoteAppsProxyLacros::AshRemoteAppsVersionForTests() const {
  // This implementation assumes that `ash_remote_apps_remote_.QueryVersion` was
  // called before because by default `mojo::Remote` has its version set to 0.
  CHECK(is_ash_remote_apps_remote_version_known_);
  return ash_remote_apps_remote_.version();
}

}  // namespace chromeos
