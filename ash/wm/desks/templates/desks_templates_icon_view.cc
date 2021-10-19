// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_icon_view.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

DesksTemplatesIconView::DesksTemplatesIconView() = default;

DesksTemplatesIconView::~DesksTemplatesIconView() = default;

void DesksTemplatesIconView::LoadIcon() {
  DCHECK(!icon_identifier_.empty());

  auto* shell_delegate = Shell::Get()->shell_delegate();
  if (is_url_) {
    shell_delegate->GetFaviconForUrl(
        icon_identifier_,
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

void DesksTemplatesIconView::OnFaviconLoaded(
    const favicon_base::FaviconImageResult& image_result) {
  if (!image_result.image.IsEmpty())
    SetImage(image_result.image.ToImageSkia());
  // TODO(chinsenj): If the favicon is null, use a placeholder.
}

void DesksTemplatesIconView::OnAppIconLoaded(
    apps::mojom::IconValuePtr icon_value) {
  gfx::ImageSkia image_result = icon_value->uncompressed;
  if (!image_result.isNull())
    SetImage(image_result);
  // TODO(chinsenj): If app icon is null, use a placeholder.
}

BEGIN_METADATA(DesksTemplatesIconView, views::ImageView)
END_METADATA

}  // namespace ash
