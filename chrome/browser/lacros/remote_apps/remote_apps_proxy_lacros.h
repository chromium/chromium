// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_H_
#define CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_H_

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

class Profile;

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace chromeos {

// A KeyedService which provides a Mojo proxy for `RemoteAppsFactory` and
// `RemoteApps` for Lacros. The RemoteApps private Mojo API creates a Mojo pipe
// directly to the extension renderer process. For security concerns, we wish
// to avoid connecting this pipe to the Ash browser process.
// `RemoteAppsProxyLacros` acts as intermediary in the Lacros browser process,
// and forwards the Mojo messages to the Ash implementation.
//
// The message flow is:
// extension(lacros) <-> RemoteAppsLacrosProxy(lacros)  <-> RemoteAppsImpl(ash)
class RemoteAppsProxyLacros
    : public remote_apps::mojom::RemoteApps,
      public remote_apps::mojom::RemoteAppLaunchObserver,
      public remote_apps::mojom::RemoteAppsFactory,
      public KeyedService {
 public:
  static constexpr char kErrorNoAshRemoteConnected[] =
      "No Ash remote connected";
  static constexpr char kErrorSetPinnedAppsNotAvailable[] =
      "Ash version of remote apps API doesn't support SetPinnedApps method";

  static std::unique_ptr<RemoteAppsProxyLacros> CreateForTesting(
      Profile* profile,
      mojo::Remote<remote_apps::mojom::RemoteAppsLacrosBridge>&
          remote_apps_lacros_bridge);

  explicit RemoteAppsProxyLacros(Profile* profile);

  RemoteAppsProxyLacros(const RemoteAppsProxyLacros&) = delete;
  RemoteAppsProxyLacros& operator=(const RemoteAppsProxyLacros&) = delete;

  ~RemoteAppsProxyLacros() override;

  void BindFactoryInterface(
      mojo::PendingReceiver<remote_apps::mojom::RemoteAppsFactory>
          pending_remote_apps_factory);

  // remote_apps::mojom::RemoteAppsFactory:
  void BindRemoteAppsAndAppLaunchObserver(
      const std::string& source_id,
      mojo::PendingReceiver<remote_apps::mojom::RemoteApps> pending_remote_apps,
      mojo::PendingRemote<remote_apps::mojom::RemoteAppLaunchObserver>
          pending_observer) override;

  // remote_apps::mojom::RemoteApps:
  void AddFolder(const std::string& name,
                 bool add_to_front,
                 AddFolderCallback callback) override;
  void AddApp(const std::string& source_id,
              const std::string& name,
              const std::string& folder_id,
              const GURL& icon_url,
              bool add_to_front,
              AddAppCallback callback) override;
  void DeleteApp(const std::string& app_id,
                 DeleteAppCallback callback) override;
  void SortLauncherWithRemoteAppsFirst(
      SortLauncherWithRemoteAppsFirstCallback callback) override;
  void SetPinnedApps(const std::vector<std::string>& app_ids,
                     SetPinnedAppsCallback callback) override;

  // remote_apps::mojom::RemoteAppLaunchObserver:
  void OnRemoteAppLaunched(const std::string& app_id,
                           const std::string& source_id) override;

  uint32_t AshRemoteAppsVersionForTests() const;

 private:
  using RemoteIds = std::map<std::string, mojo::RemoteSetElementId>;

  // Private constructor which allows setting of the `RemoteAppsLacrosBridge`
  // remote for testing. Called by `CreateForTesting()`.
  explicit RemoteAppsProxyLacros(
      Profile* profile,
      mojo::Remote<remote_apps::mojom::RemoteAppsLacrosBridge>&
          remote_apps_lacros_bridge);

  void DisconnectHandler(mojo::RemoteSetElementId id);

  void OnVersionForAppPinningReady(const std::vector<std::string>& app_ids,
                                   SetPinnedAppsCallback callback,
                                   uint32_t interface_version);
  void SetPinnedAppsImpl(const std::vector<std::string>& app_ids,
                         SetPinnedAppsCallback callback,
                         uint32_t interface_version);

  // Endpoints to communicate with extensions.
  mojo::ReceiverSet<remote_apps::mojom::RemoteAppsFactory>
      proxy_factory_receivers_;
  mojo::ReceiverSet<remote_apps::mojom::RemoteApps>
      proxy_remote_apps_receivers_;
  mojo::RemoteSet<remote_apps::mojom::RemoteAppLaunchObserver>
      proxy_app_launch_observers_;

  // Endpoints to communicate with Ash.
  mojo::Remote<remote_apps::mojom::RemoteApps> ash_remote_apps_remote_;
  mojo::Receiver<remote_apps::mojom::RemoteAppLaunchObserver>
      ash_observer_receiver_{this};

  raw_ptr<extensions::EventRouter> event_router_ = nullptr;
  RemoteIds source_id_to_remote_id_map_;
  bool is_ash_remote_apps_remote_version_known_ = false;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_H_
