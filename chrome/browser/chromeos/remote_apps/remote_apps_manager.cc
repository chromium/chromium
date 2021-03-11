// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/remote_apps/remote_apps_manager.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/image_downloader.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/chromeos/remote_apps/remote_apps_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia.h"

namespace chromeos {

namespace {

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("remote_apps_image_downloader", R"(
        semantics {
          sender: "Remote Apps Manager"
          description: "Fetches icons for Remote Apps."
          trigger:
            "Triggered when a Remote App is added to the ChromeOS launcher. "
            "Remote Apps can only be added by allowlisted extensions "
            "installed by enterprise policy."
          data: "No user data."
          destination: OTHER
          destination_other: "Icon URL of the Remote App"
        }
        policy {
          cookies_allowed: NO
          setting: "This request cannot be disabled."
          policy_exception_justification:
            "This request is only performed by allowlisted extensions "
            "installed by enterprise policy."
        }
      )");

class ImageDownloaderImpl : public RemoteAppsManager::ImageDownloader {
 public:
  ImageDownloaderImpl() = default;
  ImageDownloaderImpl(const ImageDownloaderImpl&) = delete;
  ImageDownloaderImpl& operator=(const ImageDownloaderImpl&) = delete;
  ~ImageDownloaderImpl() override = default;

  void Download(const GURL& url, DownloadCallback callback) override {
    ash::ImageDownloader* image_downloader = ash::ImageDownloader::Get();
    DCHECK(image_downloader);
    image_downloader->Download(url, kTrafficAnnotation, std::move(callback));
  }
};

// Placeholder icon which shows the first letter of the app's name on top of a
// gray circle.
class RemoteAppsPlaceholderIcon : public gfx::CanvasImageSource {
 public:
  RemoteAppsPlaceholderIcon(const std::string& name, int32_t size)
      : gfx::CanvasImageSource(gfx::Size(size, size)) {
    std::u16string sanitized_name = base::UTF8ToUTF16(std::string(name));
    base::i18n::UnadjustStringForLocaleDirection(&sanitized_name);
    letter_ = sanitized_name.substr(0, 1);

    if (size <= 16)
      font_style_ = ui::ResourceBundle::SmallFont;
    else if (size <= 32)
      font_style_ = ui::ResourceBundle::MediumFont;
    else
      font_style_ = ui::ResourceBundle::LargeFont;
  }
  RemoteAppsPlaceholderIcon(const RemoteAppsPlaceholderIcon&) = delete;
  RemoteAppsPlaceholderIcon& operator=(const RemoteAppsPlaceholderIcon&) =
      delete;
  ~RemoteAppsPlaceholderIcon() override = default;

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {
    const gfx::Size& icon_size = size();
    float width = static_cast<float>(icon_size.width());
    float height = static_cast<float>(icon_size.height());

    // Draw gray circle.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(SK_ColorGRAY);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(width / 2, height / 2), width / 2, flags);

    // Draw the letter on top.
    canvas->DrawStringRectWithFlags(
        letter_,
        ui::ResourceBundle::GetSharedInstance().GetFontList(font_style_),
        SK_ColorWHITE, gfx::Rect(icon_size.width(), icon_size.height()),
        gfx::Canvas::TEXT_ALIGN_CENTER);
  }

  // The first letter of the app's name.
  std::u16string letter_;
  ui::ResourceBundle::FontStyle font_style_ = ui::ResourceBundle::MediumFont;
};

}  // namespace

RemoteAppsManager::RemoteAppsManager(Profile* profile)
    : profile_(profile),
      remote_apps_(std::make_unique<apps::RemoteApps>(profile_, this)),
      model_(std::make_unique<RemoteAppsModel>()),
      image_downloader_(std::make_unique<ImageDownloaderImpl>()) {
  app_list_syncable_service_ =
      app_list::AppListSyncableServiceFactory::GetForProfile(profile_);
  model_updater_ = app_list_syncable_service_->GetModelUpdater();
  app_list_model_updater_observer_.Add(model_updater_);

  // |AppListSyncableService| manages the Chrome side AppList and has to be
  // initialized before apps can be added.
  if (app_list_syncable_service_->IsInitialized()) {
    Initialize();
  } else {
    app_list_syncable_service_observer_.Add(app_list_syncable_service_);
  }
}

RemoteAppsManager::~RemoteAppsManager() = default;

void RemoteAppsManager::Initialize() {
  DCHECK(app_list_syncable_service_->IsInitialized());
  is_initialized_ = true;
}

void RemoteAppsManager::AddApp(const std::string& name,
                               const std::string& folder_id,
                               const GURL& icon_url,
                               bool add_to_front,
                               AddAppCallback callback) {
  if (!is_initialized_) {
    std::move(callback).Run(std::string(), RemoteAppsError::kNotReady);
    return;
  }

  if (!folder_id.empty() && !model_->HasFolder(folder_id)) {
    std::move(callback).Run(std::string(),
                            RemoteAppsError::kFolderIdDoesNotExist);
    return;
  }

  // Disable |add_to_front| if app has a parent folder.
  if (!folder_id.empty())
    add_to_front = false;

  const RemoteAppsModel::AppInfo& info =
      model_->AddApp(name, icon_url, folder_id, add_to_front);
  add_app_callback_map_.insert({info.id, std::move(callback)});
  remote_apps_->AddApp(info);
}

RemoteAppsError RemoteAppsManager::DeleteApp(const std::string& id) {
  // Check if app was added but |HandleOnAppAdded| has not been called.
  if (!model_->HasApp(id) ||
      add_app_callback_map_.find(id) != add_app_callback_map_.end())
    return RemoteAppsError::kAppIdDoesNotExist;

  model_->DeleteApp(id);
  remote_apps_->DeleteApp(id);
  return RemoteAppsError::kNone;
}

std::string RemoteAppsManager::AddFolder(const std::string& folder_name,
                                         bool add_to_front) {
  const RemoteAppsModel::FolderInfo& folder_info =
      model_->AddFolder(folder_name, add_to_front);
  return folder_info.id;
}

RemoteAppsError RemoteAppsManager::DeleteFolder(const std::string& folder_id) {
  if (!model_->HasFolder(folder_id))
    return RemoteAppsError::kFolderIdDoesNotExist;

  // Move all items out of the folder. Empty folders are automatically deleted.
  RemoteAppsModel::FolderInfo& folder_info = model_->GetFolderInfo(folder_id);
  for (const auto& app : folder_info.items)
    model_updater_->MoveItemToFolder(app, std::string());
  model_->DeleteFolder(folder_id);
  return RemoteAppsError::kNone;
}

bool RemoteAppsManager::ShouldAddToFront(const std::string& id) const {
  if (model_->HasApp(id))
    return model_->GetAppInfo(id).add_to_front;

  if (model_->HasFolder(id))
    return model_->GetFolderInfo(id).add_to_front;

  return false;
}

void RemoteAppsManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void RemoteAppsManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void RemoteAppsManager::BindInterface(
    mojo::PendingReceiver<remote_apps::mojom::RemoteAppsFactory>
        pending_remote_apps_factory) {
  receivers_.Add(this, std::move(pending_remote_apps_factory));
}

void RemoteAppsManager::Shutdown() {}

void RemoteAppsManager::Create(
    mojo::PendingReceiver<remote_apps::mojom::RemoteApps> pending_remote_apps,
    mojo::PendingRemote<remote_apps::mojom::RemoteAppLaunchObserver>
        pending_observer) {
  remote_apps_impl_.Bind(std::move(pending_remote_apps),
                         std::move(pending_observer));
}

const std::map<std::string, RemoteAppsModel::AppInfo>&
RemoteAppsManager::GetApps() {
  return model_->GetAllAppInfo();
}

void RemoteAppsManager::LaunchApp(const std::string& id) {
  for (Observer& observer : observer_list_)
    observer.OnAppLaunched(id);
  remote_apps_impl_.OnAppLaunched(id);
}

gfx::ImageSkia RemoteAppsManager::GetIcon(const std::string& id) {
  if (!model_->HasApp(id))
    return gfx::ImageSkia();

  return model_->GetAppInfo(id).icon;
}

gfx::ImageSkia RemoteAppsManager::GetPlaceholderIcon(const std::string& id,
                                                     int32_t size_hint_in_dip) {
  if (!model_->HasApp(id))
    return gfx::ImageSkia();

  gfx::ImageSkia icon(std::make_unique<RemoteAppsPlaceholderIcon>(
                          model_->GetAppInfo(id).name, size_hint_in_dip),
                      gfx::Size(size_hint_in_dip, size_hint_in_dip));
  icon.EnsureRepsForSupportedScales();
  return icon;
}

apps::mojom::MenuItemsPtr RemoteAppsManager::GetMenuModel(
    const std::string& id) {
  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();
  // TODO(jityao): Temporary string for menu item.
  apps::AddCommandItem(ash::LAUNCH_NEW, IDS_APP_CONTEXT_MENU_ACTIVATE_ARC,
                       &menu_items);
  return menu_items;
}

void RemoteAppsManager::OnSyncModelUpdated() {
  DCHECK(!is_initialized_);
  Initialize();
  app_list_syncable_service_observer_.RemoveAll();
}

void RemoteAppsManager::OnAppListItemAdded(ChromeAppListItem* item) {
  if (item->is_folder() || item->is_page_break())
    return;

  // Make a copy of id as item->metadata can be invalidated.
  HandleOnAppAdded(std::string(item->id()));
}

void RemoteAppsManager::SetRemoteAppsForTesting(
    std::unique_ptr<apps::RemoteApps> remote_apps) {
  remote_apps_ = std::move(remote_apps);
}

void RemoteAppsManager::SetImageDownloaderForTesting(
    std::unique_ptr<ImageDownloader> image_downloader) {
  image_downloader_ = std::move(image_downloader);
}

RemoteAppsModel* RemoteAppsManager::GetModelForTesting() {
  return model_.get();
}

void RemoteAppsManager::SetIsInitializedForTesting(bool is_initialized) {
  is_initialized_ = is_initialized;
}

void RemoteAppsManager::HandleOnAppAdded(const std::string& id) {
  if (!model_->HasApp(id))
    return;
  RemoteAppsModel::AppInfo& app_info = model_->GetAppInfo(id);

  const std::string& folder_id = app_info.folder_id;
  // If folder was deleted, |folder_id| would be set to empty by the model, so
  // we don't have to check if it was deleted.
  if (!folder_id.empty()) {
    bool folder_already_exists = model_updater_->FindFolderItem(folder_id);
    model_updater_->MoveItemToFolder(id, folder_id);
    RemoteAppsModel::FolderInfo& folder_info = model_->GetFolderInfo(folder_id);

    if (!folder_already_exists) {
      // Update metadata for newly created folder.
      ChromeAppListItem* item = model_updater_->FindFolderItem(folder_id);
      DCHECK(item) << "Missing folder item for folder_id: " << folder_id;
      item->SetName(folder_info.name);
      item->SetIsPersistent(true);

      if (folder_info.add_to_front) {
        item->SetPosition(model_updater_->GetPositionBeforeFirstItem());
      } else {
        item->SetPosition(model_updater_->GetFirstAvailablePosition());
      }
    }
  }

  StartIconDownload(id, app_info.icon_url);

  auto it = add_app_callback_map_.find(id);
  DCHECK(it != add_app_callback_map_.end())
      << "Missing callback for id: " << id;
  std::move(it->second).Run(id, RemoteAppsError::kNone);
  add_app_callback_map_.erase(it);
}

void RemoteAppsManager::StartIconDownload(const std::string& id,
                                          const GURL& icon_url) {
  image_downloader_->Download(
      icon_url, base::BindOnce(&RemoteAppsManager::OnIconDownloaded,
                               weak_factory_.GetWeakPtr(), id));
}

void RemoteAppsManager::OnIconDownloaded(const std::string& id,
                                         const gfx::ImageSkia& icon) {
  // App may have been deleted.
  if (!model_->HasApp(id))
    return;

  RemoteAppsModel::AppInfo& app_info = model_->GetAppInfo(id);
  app_info.icon = icon;
  remote_apps_->UpdateAppIcon(id);
}

}  // namespace chromeos
