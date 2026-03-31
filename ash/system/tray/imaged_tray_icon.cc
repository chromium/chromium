// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/imaged_tray_icon.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/events/event.h"
#include "ui/views/controls/image_view.h"

namespace ash {

////////////////////////////////////////////////////////////////////////////////
// ImagedTrayIcon::IconVisibilityHandler

// A helper class that observes session state changes and updates the
// visibility of the tray icon based on a provided callback.
class ImagedTrayIcon::IconVisibilityHandler : public SessionObserver {
 public:
  explicit IconVisibilityHandler(ImagedTrayIcon* tray_icon)
      : tray_icon_(tray_icon) {
    CHECK(tray_icon_);
  }

  IconVisibilityHandler(const IconVisibilityHandler&) = delete;
  IconVisibilityHandler& operator=(const IconVisibilityHandler&) = delete;

  ~IconVisibilityHandler() override = default;

  void set_visibility_callback(IconVisibilityCallback callback) {
    visibility_callback_ = std::move(callback);
    if (!visibility_callback_.is_null()) {
      tray_icon_->SetVisiblePreferred(visibility_callback_.Run(
          Shell::Get()->session_controller()->GetSessionState()));
    }
  }

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override {
    if (!visibility_callback_.is_null()) {
      tray_icon_->SetVisiblePreferred(visibility_callback_.Run(state));
    }
  }

 private:
  const raw_ptr<ImagedTrayIcon> tray_icon_;
  IconVisibilityCallback visibility_callback_;
  ScopedSessionObserver session_observer_{this};
};

////////////////////////////////////////////////////////////////////////////////
// ImagedTrayIcon

ImagedTrayIcon::ImagedTrayIcon(Shelf* shelf,
                               const ui::ImageModel& image_model,
                               const std::u16string& tooltip,
                               const TrayBackgroundViewCatalogName catalog_name)
    : TrayBackgroundView(shelf,
                         catalog_name,
                         RoundedCornerBehavior::kAllRounded),
      session_visibility_handler_(
          std::make_unique<IconVisibilityHandler>(this)) {
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

void ImagedTrayIcon::set_icon_visibility_callback(
    IconVisibilityCallback callback) {
  session_visibility_handler_->set_visibility_callback(std::move(callback));
}

void ImagedTrayIcon::ClickedOutsideBubble(const ui::LocatedEvent& event) {}
void ImagedTrayIcon::UpdateTrayItemColor(bool is_active) {}
void ImagedTrayIcon::HandleLocaleChange() {}
void ImagedTrayIcon::HideBubbleWithView(const TrayBubbleView* bubble_view) {}
void ImagedTrayIcon::HideBubble(const TrayBubbleView* bubble_view) {}

BEGIN_METADATA(ImagedTrayIcon)
END_METADATA

}  // namespace ash
