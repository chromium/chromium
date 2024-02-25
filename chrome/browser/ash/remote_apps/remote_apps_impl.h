// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_IMPL_H_
#define CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_IMPL_H_

#include <map>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/remote_apps/remote_apps_types.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace extensions {
class Extension;
}  // namespace extensions

namespace ash {

class RemoteAppsManager;

// Forwards calls to `RemoteAppsManager` via mojo. `RemoteAppsImpl` is also in
// charge of managing the mapping between apps and extensions so app launch
// events are dispatched only to the extension which added the app.
// The Mojo API is tested under
// //chrome/browser/chromeos/extensions/remote_apps_apitest.cc
class RemoteAppsImpl : public chromeos::remote_apps::mojom::RemoteApps {
 public:
  static bool IsMojoPrivateApiAllowed(
      content::RenderFrameHost* render_frame_host,
      const extensions::Extension* extension);

  static void SetBypassChecksForTesting(bool bypass_checks_for_testing);

  explicit RemoteAppsImpl(RemoteAppsManager* manager);
  RemoteAppsImpl(const RemoteAppsImpl&) = delete;
  RemoteAppsImpl& operator=(const RemoteAppsImpl&) = delete;
  ~RemoteAppsImpl() override;

  void BindRemoteAppsAndAppLaunchObserver(
      const std::optional<std::string>& source_id,
      mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteApps>
          pending_remote_apps,
      mojo::PendingRemote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
          pending_observer);

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

  void OnAppLaunched(const std::string& source_id, const std::string& app_id);

 private:
  using SourceToRemoteIds = std::map<std::string, mojo::RemoteSetElementId>;

  void OnAppAdded(AddAppCallback callback,
                  const std::string& app_id,
                  RemoteAppsError error);

  void DisconnectHandler(mojo::RemoteSetElementId id);

  raw_ptr<RemoteAppsManager> manager_ = nullptr;
  SourceToRemoteIds source_id_to_remote_id_map_;
  mojo::ReceiverSet<chromeos::remote_apps::mojom::RemoteApps> receivers_;
  // Observers with an associated source in `source_id_to_remote_id_map_`.
  mojo::RemoteSet<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      app_launch_observers_with_source_id_;
  // Observers with no associated source. These observers will received all
  // `OnRemoteAppLaunched` events.
  mojo::RemoteSet<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
      app_launch_broadcast_observers_;
  base::WeakPtrFactory<RemoteAppsImpl> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_IMPL_H_
