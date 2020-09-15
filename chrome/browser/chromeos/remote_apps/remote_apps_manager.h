// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REMOTE_APPS_REMOTE_APPS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_REMOTE_APPS_REMOTE_APPS_MANAGER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/apps/app_service/remote_apps.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_impl.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_model.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_types.h"
#include "chrome/browser/ui/app_list/app_list_model_updater_observer.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/chrome_app_list_model_updater.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class AppListModelUpdater;
class ChromeAppListItem;
class Profile;

namespace apps {
class AppUpdate;
}  // namespace apps

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace chromeos {

class RemoteAppsImpl;

// KeyedService which manages the logic for |AppType::kRemote| in AppService.
// This service is only created for Managed Guest Sessions.
// The IDs of the added apps and folders are GUIDs generated using
// |base::GenerateGUID()|.
// See crbug.com/1101208 for more details on Remote Apps.
class RemoteAppsManager : public KeyedService,
                          public apps::RemoteApps::Delegate,
                          public app_list::AppListSyncableService::Observer,
                          public AppListModelUpdaterObserver,
                          public remote_apps::mojom::RemoteAppsFactory {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Invoked when an app is launched. |id| is the ID of the app.
    virtual void OnAppLaunched(const std::string& id) {}
  };

  class ImageDownloader {
   public:
    virtual ~ImageDownloader() = default;

    using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;
    virtual void Download(const GURL& url, DownloadCallback callback) = 0;
  };

  explicit RemoteAppsManager(Profile* profile);
  RemoteAppsManager(const RemoteAppsManager&) = delete;
  RemoteAppsManager& operator=(const RemoteAppsManager&) = delete;
  ~RemoteAppsManager() override;

  bool is_initialized() const { return is_initialized_; }

  void BindInterface(
      mojo::PendingReceiver<remote_apps::mojom::RemoteAppsFactory>
          pending_remote_apps_factory);

  using AddAppCallback =
      base::OnceCallback<void(const std::string& id, RemoteAppsError error)>;

  // Adds a app with the given |name|. If |folder_id| is non-empty, the app is
  // added to the folder with the given ID. The icon of the app is an image
  // retrieved from |icon_url| and is retrieved asynchronously. If the icon has
  // not been downloaded, or there is an error in downloading the icon, a
  // placeholder icon will be used.
  // The callback will be run with the ID of the added app, or an error if
  // there is one.
  // Adding to a non-existent folder will result in an error.
  // Adding an app before the manager is initialized will result in an error.
  void AddApp(const std::string& name,
              const std::string& folder_id,
              const GURL& icon_url,
              AddAppCallback callback);

  // Deletes the app with id |id|.
  // Deleting a non-existent app will result in an error.
  RemoteAppsError DeleteApp(const std::string& id);

  // Adds a folder with |folder_name|. Note that empty folders are not
  // shown in the launcher. Returns the ID for the added folder.
  std::string AddFolder(const std::string& folder_name);

  // Deletes the folder with id |folder_id|. All items in the folder are moved
  // to the top-level in the launcher.
  // Deleting a non-existent folder will result in an error.
  RemoteAppsError DeleteFolder(const std::string& folder_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // KeyedService:
  void Shutdown() override;

  // remote_apps::mojom::RemoteAppsFactory:
  void Create(
      mojo::PendingReceiver<remote_apps::mojom::RemoteApps> pending_remote_apps,
      mojo::PendingRemote<remote_apps::mojom::RemoteAppLaunchObserver>
          pending_observer) override;

  // apps::RemoteApps::Delegate:
  const std::map<std::string, RemoteAppsModel::AppInfo>& GetApps() override;
  void LaunchApp(const std::string& id) override;
  gfx::ImageSkia GetIcon(const std::string& id) override;
  gfx::ImageSkia GetPlaceholderIcon(const std::string& id,
                                    int32_t size_hint_in_dip) override;
  apps::mojom::MenuItemsPtr GetMenuModel(const std::string& id) override;

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override;

  // AppListModelUpdaterObserver:
  void OnAppListItemAdded(ChromeAppListItem* item) override;

  void SetRemoteAppsForTesting(std::unique_ptr<apps::RemoteApps> remote_apps);

  void SetImageDownloaderForTesting(
      std::unique_ptr<ImageDownloader> image_downloader);

  RemoteAppsModel* GetModelForTesting();

  void SetIsInitializedForTesting(bool is_initialized);

 private:
  void Initialize();

  void HandleOnAppAdded(const std::string& id);

  void HandleOnFolderCreated(const std::string& folder_id);

  void StartIconDownload(const std::string& id, const GURL& icon_url);

  void OnIconDownloaded(const std::string& id, const gfx::ImageSkia& icon);

  Profile* profile_ = nullptr;
  bool is_initialized_ = false;
  app_list::AppListSyncableService* app_list_syncable_service_ = nullptr;
  AppListModelUpdater* model_updater_ = nullptr;
  std::unique_ptr<apps::RemoteApps> remote_apps_;
  RemoteAppsImpl remote_apps_impl_{this};
  std::unique_ptr<RemoteAppsModel> model_;
  std::unique_ptr<ImageDownloader> image_downloader_;
  base::ObserverList<Observer> observer_list_;
  mojo::ReceiverSet<remote_apps::mojom::RemoteAppsFactory> receivers_;
  // Map from id to callback. The callback is run after |OnAppUpdate| for the
  // app has been observed.
  std::map<std::string, AddAppCallback> add_app_callback_map_;
  ScopedObserver<app_list::AppListSyncableService,
                 app_list::AppListSyncableService::Observer,
                 &app_list::AppListSyncableService::AddObserverAndStart>
      app_list_syncable_service_observer_{this};
  ScopedObserver<AppListModelUpdater, AppListModelUpdaterObserver>
      app_list_model_updater_observer_{this};
  base::WeakPtrFactory<RemoteAppsManager> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_REMOTE_APPS_REMOTE_APPS_MANAGER_H_
