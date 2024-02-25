// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MANAGER_H_
#define CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MANAGER_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/publishers/remote_apps.h"
#include "chrome/browser/ash/app_list/app_list_model_updater_observer.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/chrome_app_list_model_updater.h"
#include "chrome/browser/ash/remote_apps/remote_apps_impl.h"
#include "chrome/browser/ash/remote_apps/remote_apps_model.h"
#include "chrome/browser/ash/remote_apps/remote_apps_types.h"
#include "chromeos/components/remote_apps/mojom/remote_apps.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

class AppListModelUpdater;
class ChromeAppListItem;
class Profile;

namespace apps {
struct MenuItems;
}  // namespace apps

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {
class EventRouter;
}  // namespace extensions

namespace ash {

class RemoteAppsImpl;

// KeyedService which manages the logic for |AppType::kRemote| in AppService.
// This service is created for Managed Guest Sessions and Regular User Sessions.
// The IDs of the added apps and folders are GUIDs generated using
// |base::Uuid::GenerateRandomV4().AsLowercaseString()|.
// See crbug.com/1101208 for more details on Remote Apps.
class RemoteAppsManager
    : public KeyedService,
      public apps::RemoteApps::Delegate,
      public app_list::AppListSyncableService::Observer,
      public AppListModelUpdaterObserver,
      public chromeos::remote_apps::mojom::RemoteAppsFactory,
      public chromeos::remote_apps::mojom::RemoteAppsLacrosBridge {
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

  void BindFactoryInterface(
      mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteAppsFactory>
          pending_remote_apps_factory);

  void BindLacrosBridgeInterface(
      mojo::PendingReceiver<
          chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>
          pending_remote_apps_lacros_bridge);

  using AddAppCallback =
      base::OnceCallback<void(const std::string& id, RemoteAppsError error)>;

  // Adds a app with the given `name`. If `folder_id` is non-empty, the app is
  // added to the folder with the given ID. The icon of the app is an image
  // retrieved from `icon_url` and is retrieved asynchronously. If the icon has
  // not been downloaded, or there is an error in downloading the icon, a
  // placeholder icon will be used. If `add_to_front` is true and the app has
  // no parent folder, the app will be added to the front of the app item list.
  // `source_id` is a string used to identify the caller of this method. This
  // identifier is typically an extension or app ID.
  // The callback will be run with the ID of the added app, or an error if
  // there is one.
  // Adding to a non-existent folder will result in an error.
  // Adding an app before the manager is initialized will result in an error.
  void AddApp(const std::string& source_id,
              const std::string& name,
              const std::string& folder_id,
              const GURL& icon_url,
              bool add_to_front,
              AddAppCallback callback);

  // Adds a folder if the specified folder is missing in `model_updater_`.
  void MaybeAddFolder(const std::string& folder_id);

  // Returns a const pointer to the info of the specified app. If the app does
  // not exist, returns a nullptr.
  const RemoteAppsModel::AppInfo* GetAppInfo(const std::string& app_id) const;

  // Deletes the app with id |id|.
  // Deleting a non-existent app will result in an error.
  RemoteAppsError DeleteApp(const std::string& id);

  // Sorts the launcher items with the custom kAlphabeticalEphemeralAppFirst
  // sort order which moves the remote apps to the front of the launcher.
  void SortLauncherWithRemoteAppsFirst();

  // Sets the list of apps to be pinned on the shelf. If `app_ids` are empty
  // it should unpin all currently pinned apps.
  RemoteAppsError SetPinnedApps(const std::vector<std::string>& app_ids);

  // Adds a folder with |folder_name|. Note that empty folders are not shown in
  // the launcher. Returns the ID for the added folder. If |add_to_front| is
  // true, the folder will be added to the front of the app item list.
  std::string AddFolder(const std::string& folder_name, bool add_to_front);

  // Deletes the folder with id |folder_id|. All items in the folder are moved
  // to the top-level in the launcher.
  // Deleting a non-existent folder will result in an error.
  RemoteAppsError DeleteFolder(const std::string& folder_id);

  // Returns true if the app or folder with |id| should be added to the front
  // of the app item list.
  bool ShouldAddToFront(const std::string& id) const;

  // KeyedService:
  void Shutdown() override;

  // chromeos::remote_apps::mojom::RemoteAppsFactory:
  void BindRemoteAppsAndAppLaunchObserver(
      const std::string& source_id,
      mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteApps>
          pending_remote_apps,
      mojo::PendingRemote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
          pending_observer) override;

  // chromeos::remote_apps::mojom::RemoteAppsLacrosBridge:
  void BindRemoteAppsAndAppLaunchObserverForLacros(
      mojo::PendingReceiver<chromeos::remote_apps::mojom::RemoteApps>
          pending_remote_apps,
      mojo::PendingRemote<chromeos::remote_apps::mojom::RemoteAppLaunchObserver>
          pending_observer) override;

  // apps::RemoteApps::Delegate:
  const std::map<std::string, RemoteAppsModel::AppInfo>& GetApps() override;
  void LaunchApp(const std::string& app_id) override;
  gfx::ImageSkia GetIcon(const std::string& id) override;
  gfx::ImageSkia GetPlaceholderIcon(const std::string& id,
                                    int32_t size_hint_in_dip) override;
  apps::MenuItems GetMenuModel(const std::string& id) override;

  // app_list::AppListSyncableService::Observer:
  void OnSyncModelUpdated() override;

  // AppListModelUpdaterObserver:
  void OnAppListItemAdded(ChromeAppListItem* item) override;

  void SetImageDownloaderForTesting(
      std::unique_ptr<ImageDownloader> image_downloader);

  RemoteAppsModel* GetModelForTesting();

  RemoteAppsImpl& GetRemoteAppsImpl() { return remote_apps_impl_; }

  void SetIsInitializedForTesting(bool is_initialized);

 private:
  void Initialize();

  void HandleOnAppAdded(const std::string& id);

  void HandleOnFolderCreated(const std::string& folder_id);

  void StartIconDownload(const std::string& id, const GURL& icon_url);

  void OnIconDownloaded(const std::string& id, const gfx::ImageSkia& icon);

  raw_ptr<Profile> profile_ = nullptr;
  bool is_initialized_ = false;
  raw_ptr<app_list::AppListSyncableService> app_list_syncable_service_ =
      nullptr;
  raw_ptr<AppListModelUpdater> model_updater_ = nullptr;
  raw_ptr<extensions::EventRouter> event_router_ = nullptr;
  std::unique_ptr<apps::RemoteApps> remote_apps_;
  RemoteAppsImpl remote_apps_impl_{this};
  std::unique_ptr<RemoteAppsModel> model_;
  std::unique_ptr<ImageDownloader> image_downloader_;
  base::ObserverList<Observer> observer_list_;
  mojo::ReceiverSet<chromeos::remote_apps::mojom::RemoteAppsFactory>
      factory_receivers_;
  mojo::ReceiverSet<chromeos::remote_apps::mojom::RemoteAppsLacrosBridge>
      bridge_receivers_;
  // Map from id to callback. The callback is run after |OnAppUpdate| for the
  // app has been observed.
  std::map<std::string, AddAppCallback> add_app_callback_map_;
  std::map<std::string, std::string> app_id_to_source_id_map_;
  base::ScopedObservation<app_list::AppListSyncableService,
                          app_list::AppListSyncableService::Observer>
      app_list_syncable_service_observation_{this};
  base::ScopedObservation<AppListModelUpdater, AppListModelUpdaterObserver>
      app_list_model_updater_observation_{this};
  base::WeakPtrFactory<RemoteAppsManager> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_REMOTE_APPS_REMOTE_APPS_MANAGER_H_
