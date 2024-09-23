// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"

#include <memory>
#include <optional>
#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/manta/features.h"
#include "content/public/browser/web_ui.h"
#include "ui/display/screen.h"

namespace ash::personalization_app {

namespace {

void OnSeaPenImageDeleted(const AccountId& account_id,
                          const uint32_t image_id,
                          base::OnceCallback<void(bool success)> callback,
                          bool success) {
  if (!success) {
    LOG(WARNING) << "Failed to delete SeaPen image.";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  // Set selected wallpaper to default if the deleted image currently selected.
  auto* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  auto wallpaper_info =
      wallpaper_controller->GetWallpaperInfoForAccountId(account_id);
  if (wallpaper_info.has_value() &&
      wallpaper_info->type == WallpaperType::kSeaPen &&
      wallpaper_info->location == base::NumberToString(image_id)) {
    wallpaper_controller->SetDefaultWallpaper(
        account_id, /*show_wallpaper=*/true, std::move(callback));
  } else {
    std::move(callback).Run(/*success=*/true);
  }
}

void OnSeaPenImageSaved(const AccountId& account_id,
                        const uint32_t image_id,
                        const bool preview_mode,
                        base::OnceCallback<void(bool success)> callback,
                        bool success) {
  if (!success) {
    LOG(WARNING) << "SeaPen image failed to save, skip setting as wallpaper.";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  auto* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);
  wallpaper_controller->SetSeaPenWallpaper(account_id, image_id, preview_mode,
                                           std::move(callback));
}

}  // namespace

PersonalizationAppSeaPenProviderImpl::PersonalizationAppSeaPenProviderImpl(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : PersonalizationAppSeaPenProviderBase(
          web_ui,
          std::move(wallpaper_fetcher_delegate),
          manta::proto::FeatureName::CHROMEOS_WALLPAPER) {}

PersonalizationAppSeaPenProviderImpl::~PersonalizationAppSeaPenProviderImpl() =
    default;

void PersonalizationAppSeaPenProviderImpl::BindInterface(
    mojo::PendingReceiver<::ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(::ash::features::IsSeaPenEnabled());
  CHECK(manta::features::IsMantaServiceEnabled());
  PersonalizationAppSeaPenProviderBase::BindInterface(std::move(receiver));
}

void PersonalizationAppSeaPenProviderImpl::OnWallpaperChanged() {
  const auto* wallpaper_controller = WallpaperController::Get();
  CHECK(wallpaper_controller);

  const AccountId account_id = GetAccountId(profile_);

  // Can occur during multi user session if both users open
  // chrome://personalization and change wallpaper.
  if (wallpaper_controller->CurrentAccountId() != account_id) {
    DVLOG(1) << "Skip updating SeaPen wallpaper for non matching AccountId";
    return;
  }

  std::optional<ash::WallpaperInfo> info =
      wallpaper_controller->GetWallpaperInfoForAccountId(account_id);

  if (!info) {
    LOG(WARNING)
        << "No wallpaper info for active user. This should only happen in "
           "tests.";
    sea_pen_observer_remote_->OnSelectedSeaPenImageChanged(std::nullopt);
    return;
  }

  if (info->type != WallpaperType::kSeaPen) {
    sea_pen_observer_remote_->OnSelectedSeaPenImageChanged(std::nullopt);
    return;
  }

  const base::FilePath path(info->location);
  const std::optional<uint32_t> id = GetIdFromFileName(path);
  sea_pen_observer_remote_->OnSelectedSeaPenImageChanged(id);
}

void PersonalizationAppSeaPenProviderImpl::OnWallpaperPreviewEnded() {
  OnWallpaperChanged();
}

void PersonalizationAppSeaPenProviderImpl::SetSeaPenObserverInternal() {
  if (!wallpaper_controller_observer_.IsObserving()) {
    wallpaper_controller_observer_.Observe(WallpaperController::Get());
  }
  // Call it once to send the first wallpaper.
  OnWallpaperChanged();
}

void PersonalizationAppSeaPenProviderImpl::SelectRecentSeaPenImageInternal(
    const uint32_t image_id,
    const bool preview_mode,
    SelectRecentSeaPenImageCallback callback) {
  ash::WallpaperController* wallpaper_controller = WallpaperController::Get();
  DCHECK(wallpaper_controller);

  const std::string& user_id_hash = GetUser(profile_)->username_hash();
  if (preview_mode) {
    // Minimize inactive windows to show fullscreen preview.
    wallpaper_controller->MinimizeInactiveWindows(user_id_hash);
  } else {
    wallpaper_controller->RestoreMinimizedWindows(user_id_hash);
  }

  wallpaper_controller->SetSeaPenWallpaper(GetAccountId(profile_), image_id,
                                           preview_mode, std::move(callback));
}

bool PersonalizationAppSeaPenProviderImpl::IsManagedSeaPenEnabledInternal() {
  return ::ash::personalization_app::IsManagedSeaPenWallpaperEnabled(profile_);
}

bool PersonalizationAppSeaPenProviderImpl::
    IsManagedSeaPenFeedbackEnabledInternal() {
  return ::ash::personalization_app::IsManagedSeaPenWallpaperFeedbackEnabled(
      profile_);
}

void PersonalizationAppSeaPenProviderImpl::GetRecentSeaPenImageIdsInternal(
    GetRecentSeaPenImageIdsCallback callback) {
  auto* sea_pen_wallpaper_manager = SeaPenWallpaperManager::GetInstance();
  DCHECK(sea_pen_wallpaper_manager);
  sea_pen_wallpaper_manager->GetImageIds(GetAccountId(profile_),
                                         std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::
    GetRecentSeaPenImageThumbnailInternal(
        const uint32_t id,
        SeaPenWallpaperManager::GetImageAndMetadataCallback callback) {
  auto* sea_pen_wallpaper_manager = SeaPenWallpaperManager::GetInstance();
  DCHECK(sea_pen_wallpaper_manager);
  sea_pen_wallpaper_manager->GetImageAndMetadata(GetAccountId(profile_), id,
                                                 std::move(callback));
}

void PersonalizationAppSeaPenProviderImpl::
    ShouldShowSeaPenIntroductionDialogInternal(
        ShouldShowSeaPenIntroductionDialogCallback callback) {
  std::move(callback).Run(contextual_tooltip::ShouldShowNudge(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kSeaPenWallpaperIntroDialog,
      /*recheck_delay=*/nullptr));
}

void PersonalizationAppSeaPenProviderImpl::
    HandleSeaPenIntroductionDialogClosedInternal() {
  contextual_tooltip::HandleGesturePerformed(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kSeaPenWallpaperIntroDialog);
}

void PersonalizationAppSeaPenProviderImpl::DeleteRecentSeaPenImage(
    const uint32_t id,
    DeleteRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_image_ids_.count(id) == 0) {
    sea_pen_receiver_.ReportBadMessage("Invalid recent Sea Pen image received");
    return;
  }

  auto* sea_pen_wallpaper_manager = SeaPenWallpaperManager::GetInstance();
  DCHECK(sea_pen_wallpaper_manager);

  sea_pen_wallpaper_manager->DeleteSeaPenImage(
      GetAccountId(profile_), id,
      base::BindOnce(&OnSeaPenImageDeleted, GetAccountId(profile_), id,
                     std::move(callback)));
}

void PersonalizationAppSeaPenProviderImpl::OnFetchWallpaperDoneInternal(
    const SeaPenImage& sea_pen_image,
    const mojom::SeaPenQueryPtr& query,
    const bool preview_mode,
    base::OnceCallback<void(bool success)> callback) {
  auto* sea_pen_wallpaper_manager = SeaPenWallpaperManager::GetInstance();
  DCHECK(sea_pen_wallpaper_manager);
  const AccountId account_id = GetAccountId(profile_);
  sea_pen_wallpaper_manager->SaveSeaPenImage(
      account_id, sea_pen_image, query,
      base::BindOnce(&OnSeaPenImageSaved, account_id, sea_pen_image.id,
                     preview_mode, std::move(callback)));
}

}  // namespace ash::personalization_app
