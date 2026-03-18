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
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

std::u16string GetLocalizedString(const ImagedTrayIcon::StringVariant& value) {
  if (std::holds_alternative<int>(value)) {
    return l10n_util::GetStringUTF16(std::get<int>(value));
  }
  return std::get<std::u16string>(value);
}

}  // namespace

ImagedTrayIcon::ImagedTrayIcon(Shelf* shelf,
                               const ui::ImageModel& image_model,
                               const StringVariant& tooltip,
                               const StringVariant& accessibility_name,
                               const TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf,
                         catalog_name,
                         RoundedCornerBehavior::kAllRounded),
      tooltip_(tooltip),
      accessibility_name_(accessibility_name) {
  auto image_view =
      views::Builder<views::ImageView>()
          .SetHorizontalAlignment(views::ImageView::Alignment::kCenter)
          .SetVerticalAlignment(views::ImageView::Alignment::kCenter)
          .SetPreferredSize(gfx::Size(kTrayItemSize, kTrayItemSize))
          .SetImage(image_model)
          .SetTooltipText(GetLocalizedString(tooltip))
          .Build();

  image_view_ = tray_container()->AddChildView(std::move(image_view));
  GetViewAccessibility().SetName(GetLocalizedString(accessibility_name));
}

ImagedTrayIcon::~ImagedTrayIcon() = default;

void ImagedTrayIcon::SetTooltip(const StringVariant& tooltip) {
  if (tooltip_ == tooltip) {
    return;
  }

  tooltip_ = tooltip;
  image_view_->SetTooltipText(GetLocalizedString(tooltip_));
}

void ImagedTrayIcon::SetAccessibilityName(const StringVariant& name) {
  if (accessibility_name_ == name) {
    return;
  }

  accessibility_name_ = name;
  GetViewAccessibility().SetName(GetLocalizedString(accessibility_name_));
}

void ImagedTrayIcon::ClickedOutsideBubble(const ui::LocatedEvent& event) {}
void ImagedTrayIcon::UpdateTrayItemColor(bool is_active) {}
void ImagedTrayIcon::HideBubbleWithView(const TrayBubbleView* bubble_view) {}
void ImagedTrayIcon::HideBubble(const TrayBubbleView* bubble_view) {}

void ImagedTrayIcon::HandleLocaleChange() {
  image_view_->SetTooltipText(GetLocalizedString(tooltip_));
  GetViewAccessibility().SetName(GetLocalizedString(accessibility_name_));
}

BEGIN_METADATA(ImagedTrayIcon)
END_METADATA

}  // namespace ash
