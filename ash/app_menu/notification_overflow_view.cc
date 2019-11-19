// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_overflow_view.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ui/base/models/menu_separator_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_separator.h"
#include "ui/views/vector_icons.h"

namespace {

// Padding in dips between the overflow separator and overflow icons.
constexpr int kOverflowSeparatorToIconPadding = 8;

// Padding in dips below the overflow icons.
constexpr int kOverflowAreaBottomPadding = 12;

// Size of overflow icons in dips.
constexpr int kIconSize = 16;

// Size used for laying out overflow icons in dips to prevent clipping.
constexpr int kIconLayoutSize = kIconSize + 1;

// Padding between overflow icons in dips.
constexpr int kInterIconPadding = 8;

// Color used for |overflow_icon_|.
constexpr SkColor kOverflowIconColor = SkColorSetRGB(0x5F, 0x63, 0x60);

}  // namespace

namespace ash {

// The icon which represents a notification.
class NotificationOverflowImageView
    : public message_center::ProportionalImageView {
 public:
  NotificationOverflowImageView(const gfx::ImageSkia& image,
                                const std::string& notification_id)
      : message_center::ProportionalImageView(gfx::Size(kIconSize, kIconSize)),
        notification_id_(notification_id) {
    SetID(kNotificationOverflowIconId);
    set_owned_by_client();
    SetImage(image, gfx::Size(kIconSize, kIconSize));
  }
  ~NotificationOverflowImageView() override = default;

  const std::string& notification_id() const { return notification_id_; }

 private:
  std::string const notification_id_;

  DISALLOW_COPY_AND_ASSIGN(NotificationOverflowImageView);
};

NotificationOverflowView::NotificationOverflowView()
    : separator_(
          new views::MenuSeparator(ui::MenuSeparatorType::NORMAL_SEPARATOR)) {
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kNotificationHorizontalPadding, kOverflowAreaBottomPadding,
                  kNotificationHorizontalPadding)));
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  AddChildView(separator_);
}

NotificationOverflowView::~NotificationOverflowView() = default;

void NotificationOverflowView::AddIcon(
    const message_center::ProportionalImageView& image_view,
    const std::string& notification_id) {
  // Insert the image at the front of the list, so that it appears on the right
  // side.
  image_views_.insert(image_views_.begin(),
                      std::make_unique<NotificationOverflowImageView>(
                          image_view.image(), notification_id));
  AddChildView(image_views_.front().get());

  if (image_views_.size() > kMaxOverflowIcons) {
    if (!overflow_icon_) {
      gfx::Image icon = gfx::Image(gfx::CreateVectorIcon(
          views::kOptionsIcon, kIconSize, kOverflowIconColor));
      overflow_icon_ = std::make_unique<message_center::ProportionalImageView>(
          gfx::Size(kIconSize, kIconSize));
      overflow_icon_->SetID(kOverflowIconId);
      overflow_icon_->set_owned_by_client();
      overflow_icon_->SetImage(icon.AsImageSkia(),
                               gfx::Size(kIconSize, kIconSize));
      AddChildView(overflow_icon_.get());
    }
    RemoveChildView(image_views_.at(kMaxOverflowIcons).get());
  }
  Layout();
}

void NotificationOverflowView::RemoveIcon(const std::string& notification_id) {
  for (auto it = image_views_.begin(); it != image_views_.end(); ++it) {
    if ((*it)->notification_id() != notification_id)
      continue;

    image_views_.erase(it);
    MaybeRemoveOverflowIcon();
    Layout();
    return;
  }
}

void NotificationOverflowView::Layout() {
  separator_->SetBoundsRect(
      gfx::Rect(width(), separator_->GetPreferredSize().height()));

  int x = width() - GetInsets().right();
  const int y =
      separator_->GetPreferredSize().height() + kOverflowSeparatorToIconPadding;

  for (size_t i = 0; i < image_views_.size() && i <= kMaxOverflowIcons; ++i) {
    views::View* icon = image_views_.at(i).get();
    if (i == kMaxOverflowIcons)
      icon = overflow_icon_.get();

    x -= kIconLayoutSize;
    icon->SetBounds(x, y, kIconLayoutSize, kIconLayoutSize);
    x -= kInterIconPadding;
  }
}

gfx::Size NotificationOverflowView::CalculatePreferredSize() const {
  // This view is the last element in a MenuItemView, which means it has extra
  // padding on the bottom due to the corner radius of the root MenuItemView. If
  // the corner radius changes, |kOverflowSeparatorToIconPadding| must be
  // modified to vertically center the overflow icons.
  return gfx::Size(views::MenuConfig::instance().touchable_menu_width,
                   separator_->GetPreferredSize().height() +
                       kOverflowSeparatorToIconPadding + kIconLayoutSize);
}

void NotificationOverflowView::MaybeRemoveOverflowIcon() {
  if (!overflow_icon_ || image_views_.size() > kMaxOverflowIcons)
    return;

  overflow_icon_.reset();
}

}  // namespace ash
