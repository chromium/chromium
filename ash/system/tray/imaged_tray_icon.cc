// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/imaged_tray_icon.h"

#include <string>
#include <utility>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/views/controls/image_view.h"

namespace ash {

ImagedTrayIcon::ImagedTrayIcon(Shelf* shelf,
                               const ui::ImageModel& image_model,
                               const std::u16string& tooltip,
                               const TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf,
                         catalog_name,
                         RoundedCornerBehavior::kAllRounded) {
  auto image_view =
      views::Builder<views::ImageView>()
          .SetHorizontalAlignment(views::ImageView::Alignment::kCenter)
          .SetVerticalAlignment(views::ImageView::Alignment::kCenter)
          .SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize))
          .SetImage(image_model)
          .SetTooltipText(tooltip)
          .Build();

  image_view_ = tray_container()->AddChildView(std::move(image_view));
}

ImagedTrayIcon::~ImagedTrayIcon() = default;

void ImagedTrayIcon::ClickedOutsideBubble(const ui::LocatedEvent& event) {}
void ImagedTrayIcon::UpdateTrayItemColor(bool is_active) {}
void ImagedTrayIcon::HandleLocaleChange() {}
void ImagedTrayIcon::HideBubbleWithView(const TrayBubbleView* bubble_view) {}
void ImagedTrayIcon::HideBubble(const TrayBubbleView* bubble_view) {}

BEGIN_METADATA(ImagedTrayIcon)
END_METADATA

}  // namespace ash
