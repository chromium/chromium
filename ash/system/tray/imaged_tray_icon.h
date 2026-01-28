// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_IMAGED_TRAY_ICON_H_
#define ASH_SYSTEM_TRAY_IMAGED_TRAY_ICON_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
class ImageModel;
class LocatedEvent;
}  // namespace ui

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class Shelf;
class TrayBubbleView;

class ASH_EXPORT ImagedTrayIcon : public TrayBackgroundView {
  METADATA_HEADER(ImagedTrayIcon, TrayBackgroundView)

 public:
  ImagedTrayIcon(Shelf* shelf,
                 const ui::ImageModel& image_model,
                 const std::u16string& tooltip,
                 const TrayBackgroundViewCatalogName catalog_name);

  ImagedTrayIcon(const ImagedTrayIcon&) = delete;
  ImagedTrayIcon& operator=(const ImagedTrayIcon&) = delete;

  ~ImagedTrayIcon() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  views::ImageView* image_view() { return image_view_; }

 private:
  raw_ptr<views::ImageView> image_view_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_IMAGED_TRAY_ICON_H_
