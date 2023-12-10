// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BROWSER_SHORTCUTS_CROSAPI_PUBLISHER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BROWSER_SHORTCUTS_CROSAPI_PUBLISHER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/publishers/shortcut_publisher.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace apps {

// An shortcut publisher for browser created shortcuts. This is a proxy
// publisher that lives in ash-chrome, and the shortcuts will be published over
// crosapi. This proxy publisher will also handle reconnection when the crosapi
// connection drops.
class BrowserShortcutsCrosapiPublisher
    : public apps::ShortcutPublisher,
      public crosapi::mojom::AppShortcutPublisher {
 public:
  explicit BrowserShortcutsCrosapiPublisher(apps::AppServiceProxy* proxy);
  BrowserShortcutsCrosapiPublisher(const BrowserShortcutsCrosapiPublisher&) =
      delete;
  BrowserShortcutsCrosapiPublisher& operator=(
      const BrowserShortcutsCrosapiPublisher&) = delete;
  ~BrowserShortcutsCrosapiPublisher() override;

  // Register the browser shortcuts host from lacros-chrome to allow
  // lacros-chrome publishing browser shortcuts to app service in ash-chrome.
  void RegisterCrosapiHost(
      mojo::PendingReceiver<crosapi::mojom::AppShortcutPublisher> receiver);

  void SetLaunchShortcutCallbackForTesting(
      crosapi::mojom::AppShortcutController::LaunchShortcutCallback callback);

  void SetRemoveShortcutCallbackForTesting(
      crosapi::mojom::AppShortcutController::RemoveShortcutCallback callback);

 private:
  // crosapi::mojom::AppShortcutPublisher overrides.
  void PublishShortcuts(std::vector<apps::ShortcutPtr> deltas,
                        PublishShortcutsCallback callback) override;
  void RegisterAppShortcutController(
      mojo::PendingRemote<crosapi::mojom::AppShortcutController> controller,
      RegisterAppShortcutControllerCallback callback) override;
  void ShortcutRemoved(const std::string& shortcut_id,
                       ShortcutRemovedCallback callback) override;

  // apps::ShortcutPublisher overrides.
  void LaunchShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      int64_t display_id) override;
  void RemoveShortcut(const std::string& host_app_id,
                      const std::string& local_shortcut_id,
                      apps::UninstallSource uninstall_source) override;
  void GetCompressedIconData(const std::string& shortcut_id,
                             int32_t size_in_dip,
                             ui::ResourceScaleFactor scale_factor,
                             apps::LoadIconCallback callback) override;

  void OnCrosapiDisconnected();
  void OnControllerDisconnected();

  mojo::Receiver<crosapi::mojom::AppShortcutPublisher> receiver_{this};
  mojo::Remote<crosapi::mojom::AppShortcutController> controller_;
  const raw_ptr<apps::AppServiceProxy> proxy_;

  crosapi::mojom::AppShortcutController::LaunchShortcutCallback
      launch_shortcut_callback_for_testing_;
  crosapi::mojom::AppShortcutController::RemoveShortcutCallback
      remove_shortcut_callback_for_testing_;

  base::WeakPtrFactory<BrowserShortcutsCrosapiPublisher> weak_factory_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BROWSER_SHORTCUTS_CROSAPI_PUBLISHER_H_
