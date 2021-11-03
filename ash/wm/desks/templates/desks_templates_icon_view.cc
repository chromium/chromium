// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_icon_view.h"

#include "ash/public/cpp/rounded_image_view.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/style/ash_color_provider.h"
#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/favicon_base/favicon_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/native_theme/native_theme.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "url/gurl.h"

namespace {

// The size of the insets for the `count_label_`.
constexpr int kCountLabelInsetSize = 4;

// Return the formatted string for `count`. If `count` is <=9, the string will
// be "+<count>". If `count` is >9, the string will be "9+".
std::u16string GetCountString(int count) {
  return base::UTF8ToUTF16(count > 9 ? "9+" : base::StringPrintf("+%i", count));
}

}  // namespace

namespace ash {

DesksTemplatesIconView::DesksTemplatesIconView() = default;

DesksTemplatesIconView::~DesksTemplatesIconView() = default;

void DesksTemplatesIconView::SetIconAndCount(const std::string& icon_identifier,
                                             int count) {
  icon_identifier_ = icon_identifier;
  count_ = count;

  if (count_ > 1 || icon_identifier_.empty()) {
    DCHECK(!count_label_);
    count_label_ = AddChildView(
        views::Builder<views::Label>()
            .SetText(GetCountString(count_))
            .SetBorder(views::CreateEmptyBorder(gfx::Insets(
                kCountLabelInsetSize, kCountLabelInsetSize,
                kCountLabelInsetSize,
                icon_view_ ? 2 * kCountLabelInsetSize : kCountLabelInsetSize)))
            .SetBackgroundColor(AshColorProvider::Get()->GetControlsLayerColor(
                AshColorProvider::ControlsLayerType::
                    kControlBackgroundColorInactive))
            .SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
                AshColorProvider::ContentLayerType::kTextColorSecondary))
            .Build());
  }

  if (icon_identifier_.empty())
    return;

  DCHECK(!icon_view_);
  icon_view_ =
      AddChildView(views::Builder<RoundedImageView>()
                       .SetCornerRadius(DesksTemplatesIconView::kIconSize / 2)
                       .Build());

  GURL potential_url{icon_identifier_};
  auto* shell_delegate = Shell::Get()->shell_delegate();
  if (potential_url.is_valid()) {
    shell_delegate->GetFaviconForUrl(
        icon_identifier_, kIconSize,
        base::BindOnce(&DesksTemplatesIconView::OnFaviconLoaded,
                       weak_ptr_factory_.GetWeakPtr()),
        &cancelable_task_tracker_);
  } else {
    shell_delegate->GetIconForAppId(
        icon_identifier_, kIconSize,
        base::BindOnce(&DesksTemplatesIconView::OnAppIconLoaded,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DesksTemplatesIconView::UpdateCount(int count) {
  count_ = count;
  DCHECK(count_label_);
  count_label_->SetText(GetCountString(count_));
}

gfx::Size DesksTemplatesIconView::CalculatePreferredSize() const {
  return gfx::Size(count_ > 1 && icon_view_ ? 2 * kIconSize : kIconSize,
                   kIconSize);
}

void DesksTemplatesIconView::Layout() {
  if (icon_view_)
    icon_view_->SetBoundsRect(gfx::Rect(kIconSize, kIconSize));

  if (count_label_) {
    count_label_->SetBoundsRect(
        gfx::Rect(icon_view_ ? kIconSize : 0, 0, kIconSize, kIconSize));
  }
}

void DesksTemplatesIconView::OnFaviconLoaded(
    const favicon_base::FaviconRawBitmapResult& image_result) {
  if (image_result.is_valid()) {
    icon_view_->SetImage(
        favicon_base::SelectFaviconFramesFromPNGs(
            std::vector<favicon_base::FaviconRawBitmapResult>{image_result},
            favicon_base::GetFaviconScales(), kIconSize)
            .AsImageSkia());
    return;
  }
  LoadDefaultIcon();
}

void DesksTemplatesIconView::OnAppIconLoaded(
    apps::mojom::IconValuePtr icon_value) {
  gfx::ImageSkia image_result = icon_value->uncompressed;
  if (!image_result.isNull()) {
    icon_view_->SetImage(image_result, gfx::Size(kIconSize, kIconSize));
    return;
  }
  LoadDefaultIcon();
}

void DesksTemplatesIconView::LoadDefaultIcon() {
  const ui::NativeTheme* native_theme =
      ui::NativeTheme::GetInstanceForNativeUi();
  int resource_id = native_theme && native_theme->ShouldUseDarkColors()
                        ? IDR_DEFAULT_FAVICON_DARK
                        : IDR_DEFAULT_FAVICON;
  icon_view_->SetImage(ui::ResourceBundle::GetSharedInstance()
                           .GetImageNamed(resource_id)
                           .AsImageSkia());
}

BEGIN_METADATA(DesksTemplatesIconView, views::View)
END_METADATA

}  // namespace ash
