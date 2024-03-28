// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_app_image_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr gfx::Size kItemIconPreferredSize(32, 32);
constexpr gfx::Size kOverflowIconPreferredSize(20, 20);
constexpr int kItemIconBackgroundRounding = 10;

gfx::Size GetImageSizeForType(const PineAppImageView::Type type) {
  switch (type) {
    case PineAppImageView::Type::kScreenshot:
      return pine::kScreenshotIconRowImageViewSize;
    case PineAppImageView::Type::kItem:
      return kItemIconPreferredSize;
    case PineAppImageView::Type::kOverflow:
      return kOverflowIconPreferredSize;
  }
}

gfx::Size GetPreferredSizeForType(const PineAppImageView::Type type) {
  switch (type) {
    case PineAppImageView::Type::kScreenshot:
      return pine::kScreenshotIconRowImageViewSize;
    case PineAppImageView::Type::kItem:
      return pine::kItemIconBackgroundPreferredSize;
    case PineAppImageView::Type::kOverflow:
      return kOverflowIconPreferredSize;
  }
}

}  // namespace

PineAppImageView::PineAppImageView(const std::string& app_id, const Type type) {
  SetImageSize(GetImageSizeForType(type));
  SetPreferredSize(GetPreferredSizeForType(type));

  if (type == Type::kItem) {
    SetBackground(views::CreateThemedRoundedRectBackground(
        pine::kIconBackgroundColor, kItemIconBackgroundRounding));
  }

  // The callback may be called synchronously.
  Shell::Get()->saved_desk_delegate()->GetIconForAppId(
      app_id,
      type == Type::kScreenshot ? pine::kScreenshotIconRowIconSize
                                : pine::kAppImageSize,
      base::BindOnce(&PineAppImageView::GetIconCallback,
                     weak_ptr_factory_.GetWeakPtr()));
}

PineAppImageView::~PineAppImageView() = default;

void PineAppImageView::GetIconCallback(const gfx::ImageSkia& icon) {
  // TODO(hewer): Add a default app icon if `icon` is null.
  SetImage(ui::ImageModel::FromImageSkia(icon));
}

BEGIN_METADATA(PineAppImageView)
END_METADATA

}  // namespace ash
