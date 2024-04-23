// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_overflow_view.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "base/ranges/algorithm.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

}  // namespace

namespace ash {

// The icon which represents a notification.
class NotificationOverflowImageView
    : public message_center::ProportionalImageView {
  METADATA_HEADER(NotificationOverflowImageView,
                  message_center::ProportionalImageView)

 public:
  NotificationOverflowImageView(const ui::ImageModel& image,
                                const std::string& notification_id)
      : message_center::ProportionalImageView(gfx::Size(kIconSize, kIconSize)),
        notification_id_(notification_id) {
    SetID(kNotificationOverflowIconId);
    SetImage(image, gfx::Size(kIconSize, kIconSize));
  }

  NotificationOverflowImageView(const NotificationOverflowImageView&) = delete;
  NotificationOverflowImageView& operator=(
      const NotificationOverflowImageView&) = delete;

  ~NotificationOverflowImageView() override = default;

  const std::string& notification_id() const { return notification_id_; }

 private:
  std::string const notification_id_;
};

BEGIN_METADATA(NotificationOverflowImageView)
END_METADATA

NotificationOverflowView::NotificationOverflowView()
    : separator_(AddChildView(std::make_unique<views::MenuSeparator>(
          ui::MenuSeparatorType::NORMAL_SEPARATOR))) {
  SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, kNotificationHorizontalPadding, kOverflowAreaBottomPadding,
      kNotificationHorizontalPadding)));
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));
}

NotificationOverflowView::~NotificationOverflowView() = default;

void NotificationOverflowView::AddIcon(
    const message_center::ProportionalImageView& image_view,
    const std::string& notification_id) {
  // Insert the image at the front of the list, so that it appears on the right
  // side.
  image_views_.insert(
      image_views_.begin(),
      AddChildView(std::make_unique<NotificationOverflowImageView>(
          image_view.image(), notification_id)));

  if (image_views_.size() > kMaxOverflowIcons) {
    if (!overflow_icon_) {
      auto icon = ui::ImageModel::FromVectorIcon(views::kOptionsIcon,
                                                 ui::kColorIcon, kIconSize);
      auto overflow_icon =
          std::make_unique<message_center::ProportionalImageView>(
              gfx::Size(kIconSize, kIconSize));
      overflow_icon->SetID(kOverflowIconId);
      overflow_icon->SetImage(icon, gfx::Size(kIconSize, kIconSize));
      overflow_icon_ = AddChildView(std::move(overflow_icon));
    }
    overflow_icon_->SetVisible(true);
    image_views_.at(kMaxOverflowIcons)->SetVisible(false);
  }
  DeprecatedLayoutImmediately();
}

void NotificationOverflowView::RemoveIcon(const std::string& notification_id) {
  auto it = base::ranges::find(image_views_, notification_id,
                               &NotificationOverflowImageView::notification_id);
  if (it != image_views_.end()) {
    RemoveChildViewT(*it);
    image_views_.erase(it);
    MaybeRemoveOverflowIcon();
    DeprecatedLayoutImmediately();
  }
}

void NotificationOverflowView::Layout(PassKey) {
  separator_->SetBoundsRect(
      gfx::Rect(width(), separator_->GetPreferredSize().height()));

  int x = width() - GetInsets().right();
  const int y =
      separator_->GetPreferredSize().height() + kOverflowSeparatorToIconPadding;

  for (size_t i = 0; i < image_views_.size() && i <= kMaxOverflowIcons; ++i) {
    views::View* icon = image_views_.at(i);
    if (i == kMaxOverflowIcons)
      icon = overflow_icon_;

    x -= kIconLayoutSize;
    icon->SetBounds(x, y, kIconLayoutSize, kIconLayoutSize);
    x -= kInterIconPadding;
  }
}

gfx::Size NotificationOverflowView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // This view is the last element in a MenuItemView, which means it has extra
  // padding on the bottom due to the corner radius of the root MenuItemView. If
  // the corner radius changes, |kOverflowSeparatorToIconPadding| must be
  // modified to vertically center the overflow icons.
  return gfx::Size(views::MenuConfig::instance().touchable_menu_min_width,
                   separator_->GetPreferredSize().height() +
                       kOverflowSeparatorToIconPadding + kIconLayoutSize);
}

void NotificationOverflowView::MaybeRemoveOverflowIcon() {
  if (!overflow_icon_ || image_views_.size() > kMaxOverflowIcons)
    return;

  overflow_icon_->SetVisible(false);
}

BEGIN_METADATA(NotificationOverflowView)
END_METADATA

}  // namespace ash
