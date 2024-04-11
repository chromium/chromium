// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_app_image_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr gfx::Size kItemIconPreferredSize(32, 32);
constexpr int kItemIconBackgroundRounding = 10;

gfx::Size GetImageSizeForType(const PineAppImageView::Type type) {
  switch (type) {
    case PineAppImageView::Type::kScreenshot:
      return pine::kScreenshotIconRowImageViewSize;
    case PineAppImageView::Type::kItem:
      return kItemIconPreferredSize;
    case PineAppImageView::Type::kOverflow:
      return pine::kOverflowIconPreferredSize;
  }
}

gfx::Size GetPreferredSizeForType(const PineAppImageView::Type type) {
  switch (type) {
    case PineAppImageView::Type::kScreenshot:
      return pine::kScreenshotIconRowImageViewSize;
    case PineAppImageView::Type::kItem:
      return pine::kItemIconBackgroundPreferredSize;
    case PineAppImageView::Type::kOverflow:
      return pine::kOverflowIconPreferredSize;
  }
}

int GetIconSizeForType(const PineAppImageView::Type type) {
  switch (type) {
    case PineAppImageView::Type::kScreenshot:
      return pine::kScreenshotIconRowIconSize;
    case PineAppImageView::Type::kItem:
      return pine::kAppImageSize;
    case PineAppImageView::Type::kOverflow:
      return pine::kAppImageSize;
  }
}

}  // namespace

PineAppImageView::PineAppImageView(const std::string& app_id,
                                   const Type type,
                                   base::OnceClosure ready_callback)
    : app_id_(app_id), type_(type), ready_callback_(std::move(ready_callback)) {
  SetImageSize(GetImageSizeForType(type_));
  SetPreferredSize(GetPreferredSizeForType(type_));

  // Set a default image that may be replaced when the app icon is fetched
  // and/or the app is installed.
  SetImage(ui::ImageModel::FromVectorIcon(kDefaultAppIcon,
                                          cros_tokens::kCrosSysPrimary));

  if (type_ == Type::kItem) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        pine::kIconBackgroundColorId, kItemIconBackgroundRounding));
  }

  // The callback may be called synchronously.
  Shell::Get()->saved_desk_delegate()->GetIconForAppId(
      app_id_, GetIconSizeForType(type_),
      base::BindOnce(&PineAppImageView::GetIconCallback,
                     weak_ptr_factory_.GetWeakPtr()));

  // Observe the cache for changes to app readiness.
  auto* cache = apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
      Shell::Get()->session_controller()->GetActiveAccountId());
  if (cache && !cache->IsAppInstalled(app_id_)) {
    app_registry_cache_observer_.Observe(cache);
  }
}

PineAppImageView::~PineAppImageView() = default;

void PineAppImageView::OnAppUpdate(const apps::AppUpdate& update) {
  // If the update matches our desired App ID, and it shows a change in
  // readiness (i.e., the app has been installed), then fetch the icon again.
  if (update.AppId() == app_id_ && update.Delta() &&
      update.Delta()->readiness == apps::Readiness::kReady) {
    auto* delegate = Shell::Get()->saved_desk_delegate();

    // The callback may be called synchronously.
    delegate->GetIconForAppId(update.AppId(), GetIconSizeForType(type_),
                              base::BindOnce(&PineAppImageView::GetIconCallback,
                                             weak_ptr_factory_.GetWeakPtr()));

    // Run any callbacks that also need to occur after the app is installed
    // (e.g., updating the app title).
    std::move(ready_callback_).Run();

    // We no longer need to observe after installation.
    app_registry_cache_observer_.Reset();
  }
}

void PineAppImageView::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void PineAppImageView::GetIconCallback(const gfx::ImageSkia& icon) {
  // We don't want to replace the default icon if `icon` is null.
  if (!icon.isNull()) {
    SetImage(ui::ImageModel::FromImageSkia(icon));
  }
}

BEGIN_METADATA(PineAppImageView)
END_METADATA

}  // namespace ash
