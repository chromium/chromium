// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REMOTE_APPS_REMOTE_APPS_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_REMOTE_APPS_REMOTE_APPS_IMPL_H_

#include <set>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_types.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace extensions {
class Extension;
}

namespace chromeos {

class RemoteAppsManager;

// Forwards calls to RemoteAppsManager via mojo. Also keeps track of apps added
// by the extension for |OnAppLaunched()|.
class RemoteAppsImpl : public remote_apps::mojom::RemoteApps {
 public:
  static bool IsAllowed(content::RenderFrameHost* render_frame_host,
                        const extensions::Extension* extension);

  static void SetBypassChecksForTesting(bool bypass_checks_for_testing);

  explicit RemoteAppsImpl(RemoteAppsManager* manager);
  RemoteAppsImpl(const RemoteAppsImpl&) = delete;
  RemoteAppsImpl& operator=(const RemoteAppsImpl&) = delete;
  ~RemoteAppsImpl() override;

  void Bind(
      mojo::PendingReceiver<remote_apps::mojom::RemoteApps> pending_remote_apps,
      mojo::PendingRemote<remote_apps::mojom::RemoteAppLaunchObserver>
          pending_observer);

  // remote_apps::mojom::RemoteApps:
  void AddFolder(const std::string& name,
                 bool add_to_front,
                 AddFolderCallback callback) override;
  void AddApp(const std::string& name,
              const std::string& folder_id,
              const GURL& icon_url,
              bool add_to_front,
              AddAppCallback callback) override;

  void OnAppLaunched(const std::string& id);

 private:
  void OnAppAdded(AddAppCallback callback,
                  const std::string& id,
                  RemoteAppsError error);

  RemoteAppsManager* manager_ = nullptr;
  std::set<std::string> app_ids_;
  mojo::ReceiverSet<remote_apps::mojom::RemoteApps> receivers_;
  mojo::RemoteSet<remote_apps::mojom::RemoteAppLaunchObserver>
      app_launch_observers_;
  base::WeakPtrFactory<RemoteAppsImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_REMOTE_APPS_REMOTE_APPS_IMPL_H_
