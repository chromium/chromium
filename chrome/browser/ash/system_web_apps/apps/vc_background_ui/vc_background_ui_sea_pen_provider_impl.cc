// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/vc_background_ui/vc_background_ui_sea_pen_provider_impl.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/image_util.h"
#include "ash/shell.h"
#include "ash/system/camera/camera_effects_controller.h"
#include "ash/wallpaper/sea_pen_wallpaper_manager.h"
#include "ash/wallpaper/wallpaper_utils/sea_pen_metadata_utils.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_base.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/ui/webui/sanitized_image_source.h"
#include "components/manta/features.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "media/capture/video/chromeos/camera_hal_dispatcher_impl.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom-forward.h"
#include "ui/gfx/image/image_skia.h"

namespace ash::vc_background_ui {

namespace {

CameraEffectsController* GetCameraEffectsController() {
  auto* controller = Shell::Get()->camera_effects_controller();
  DCHECK(controller);
  return controller;
}

void OnGetBackgroundImageInfo(
    SeaPenWallpaperManager::GetImageAndMetadataCallback callback,
    const std::optional<CameraEffectsController::BackgroundImageInfo>&
        background_image_info) {
  if (!background_image_info.has_value()) {
    std::move(callback).Run(gfx::ImageSkia(), nullptr);
    return;
  }

  const std::string extracted_metadata =
      ExtractDcDescriptionContents(background_image_info->metadata);
  DecodeJsonMetadata(
      extracted_metadata.empty() ? background_image_info->metadata
                                 : extracted_metadata,
      base::BindOnce(std::move(callback), background_image_info->image));
}

}  // namespace

VcBackgroundUISeaPenProviderImpl::VcBackgroundUISeaPenProviderImpl(
    content::WebUI* web_ui,
    std::unique_ptr<wallpaper_handlers::WallpaperFetcherDelegate>
        wallpaper_fetcher_delegate)
    : PersonalizationAppSeaPenProviderBase(
          web_ui,
          std::move(wallpaper_fetcher_delegate),
          manta::proto::FeatureName::CHROMEOS_VC_BACKGROUNDS) {
  content::URLDataSource::Add(profile_,
                              std::make_unique<SanitizedImageSource>(profile_));
}

VcBackgroundUISeaPenProviderImpl::~VcBackgroundUISeaPenProviderImpl() = default;

void VcBackgroundUISeaPenProviderImpl::BindInterface(
    mojo::PendingReceiver<ash::personalization_app::mojom::SeaPenProvider>
        receiver) {
  CHECK(::ash::features::IsVcBackgroundReplaceEnabled());
  CHECK(::manta::features::IsMantaServiceEnabled());
  ::ash::personalization_app::PersonalizationAppSeaPenProviderBase::
      BindInterface(std::move(receiver));
}

void VcBackgroundUISeaPenProviderImpl::SetSeaPenObserverInternal() {
  if (!scoped_camera_effect_observation_.IsObserving()) {
    scoped_camera_effect_observation_.Observe(
        media::CameraHalDispatcherImpl::GetInstance());
  }
  // Call it once to initialize camera effects.
  OnCameraEffectChanged(GetCameraEffectsController()->GetCameraEffects());
}

void VcBackgroundUISeaPenProviderImpl::SelectRecentSeaPenImageInternal(
    const uint32_t id,
    const bool preview_mode,
    SelectRecentSeaPenImageCallback callback) {
  if (preview_mode) {
    sea_pen_receiver_.ReportBadMessage(
        "Preview mode is only used for wallpaper");
    return;
  }
  GetCameraEffectsController()->SetBackgroundImage(
      CameraEffectsController::SeaPenIdToRelativePath(id), std::move(callback));
}

bool VcBackgroundUISeaPenProviderImpl::IsManagedSeaPenEnabledInternal() {
  return ::ash::personalization_app::IsManagedSeaPenVcBackgroundEnabled(
      profile_);
}

bool VcBackgroundUISeaPenProviderImpl::
    IsManagedSeaPenFeedbackEnabledInternal() {
  return ::ash::personalization_app::IsManagedSeaPenVcBackgroundFeedbackEnabled(
      profile_);
}

void VcBackgroundUISeaPenProviderImpl::GetRecentSeaPenImageIdsInternal(
    GetRecentSeaPenImageIdsCallback callback) {
  GetCameraEffectsController()->GetBackgroundImageFileNames(
      base::BindOnce(&GetIdsFromFilePaths).Then(std::move(callback)));
}

void VcBackgroundUISeaPenProviderImpl::GetRecentSeaPenImageThumbnailInternal(
    const uint32_t id,
    SeaPenWallpaperManager::GetImageAndMetadataCallback callback) {
  GetCameraEffectsController()->GetBackgroundImageInfo(
      CameraEffectsController::SeaPenIdToRelativePath(id),
      base::BindOnce(&OnGetBackgroundImageInfo, std::move(callback)));
}

void VcBackgroundUISeaPenProviderImpl::
    ShouldShowSeaPenIntroductionDialogInternal(
        ShouldShowSeaPenIntroductionDialogCallback callback) {
  std::move(callback).Run(contextual_tooltip::ShouldShowNudge(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kSeaPenVcBackgroundIntroDialog,
      /*recheck_delay=*/nullptr));
}

void VcBackgroundUISeaPenProviderImpl::
    HandleSeaPenIntroductionDialogClosedInternal() {
  contextual_tooltip::HandleGesturePerformed(
      profile_->GetPrefs(),
      contextual_tooltip::TooltipType::kSeaPenVcBackgroundIntroDialog);
}

void VcBackgroundUISeaPenProviderImpl::DeleteRecentSeaPenImage(
    const uint32_t id,
    DeleteRecentSeaPenImageCallback callback) {
  if (recent_sea_pen_image_ids_.count(id) == 0) {
    sea_pen_receiver_.ReportBadMessage("Invalid Sea Pen image received");
    return;
  }

  GetCameraEffectsController()->RemoveBackgroundImage(
      CameraEffectsController::SeaPenIdToRelativePath(id), std::move(callback));
}

void VcBackgroundUISeaPenProviderImpl::OnCameraEffectChanged(
    const cros::mojom::EffectsConfigPtr& new_effects) {
  user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  DCHECK(active_user);

  if (active_user->GetAccountId() !=
      personalization_app::GetAccountId(profile_)) {
    DVLOG(1) << __func__ << " ignore camera effect update for other user";
    return;
  }

  if (new_effects.is_null() || !new_effects->replace_enabled ||
      !new_effects->background_filepath) {
    sea_pen_observer_remote_->OnSelectedSeaPenImageChanged(std::nullopt);
    return;
  }

  sea_pen_observer_remote_->OnSelectedSeaPenImageChanged(
      GetIdFromFileName(new_effects->background_filepath.value()));
}

void VcBackgroundUISeaPenProviderImpl::OnFetchWallpaperDoneInternal(
    const SeaPenImage& sea_pen_image,
    const ash::personalization_app::mojom::SeaPenQueryPtr& query,
    const bool preview_mode,
    base::OnceCallback<void(bool success)> callback) {
  if (preview_mode) {
    sea_pen_receiver_.ReportBadMessage(
        "Preview mode is only used for wallpaper");
    return;
  }
  const std::optional<std::string> metadata =
      base::WriteJson(SeaPenQueryToDict(query));
  if (!metadata.has_value()) {
    LOG(WARNING) << "Failed to write json metadata";
  }
  GetCameraEffectsController()->SetBackgroundImageFromContent(
      sea_pen_image, metadata.value_or(std::string()), std::move(callback));
}

}  // namespace ash::vc_background_ui
