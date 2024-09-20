// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/version_info/channel.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Background rounded rectangle corner radius.
constexpr int kIndicatorBgCornerRadius = 50;

// Size of padding area between the border and icon or text.
constexpr int kBorderInset = 6;

// Size of the vector icon.
constexpr int kVectorIconSize = 16;

// Insets for the channel indicator icon's background.
constexpr int kIconBackgroundInset = 2;

}  // namespace

ChannelIndicatorView::ChannelIndicatorView(Shelf* shelf,
                                           version_info::Channel channel)
    : TrayItemView(shelf),
      channel_(channel),
      box_layout_(SetLayoutManager(std::make_unique<views::BoxLayout>())),
      session_observer_(this) {
  shell_observer_.Observe(Shell::Get());
  SetVisible(false);

  // Set role before calling the `Update` method to ensure
  // `AccessibilityPaintChecks` pass.
  GetViewAccessibility().SetRole(ax::mojom::Role::kLabelText);

  Update();
}

ChannelIndicatorView::~ChannelIndicatorView() = default;

views::View* ChannelIndicatorView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string ChannelIndicatorView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

void ChannelIndicatorView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();

  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    // User is logged in, set image view colors.
    if (image_view()) {
      image_view()->SetBackground(views::CreateThemedRoundedRectBackground(
          channel_indicator_utils::GetBgColorJelly(channel_),
          (IsHorizontalAlignment() ? GetLocalBounds().width()
                                   : GetLocalBounds().height()) /
              2.0f));
      image_view()->SetImage(ui::ImageModel::FromVectorIcon(
          channel_indicator_utils::GetVectorIcon(channel_),
          channel_indicator_utils::GetFgColorJelly(channel_), kVectorIconSize));
    }
    return;
  }

  // User is not logged in, set label colors.
  if (label()) {
    label()->SetBackground(views::CreateThemedRoundedRectBackground(
        channel_indicator_utils::GetBgColorJelly(channel_),
        kIndicatorBgCornerRadius));
    label()->SetEnabledColorId(
        channel_indicator_utils::GetFgColorJelly(channel_));
  }
}

void ChannelIndicatorView::HandleLocaleChange() {
  Update();
}

void ChannelIndicatorView::Update() {
  if (!channel_indicator_utils::IsDisplayableChannel(channel_))
    return;

  SetImageOrText();
  SetVisible(true);
  SetTooltip();

  DCHECK(channel_indicator_utils::IsDisplayableChannel(channel_));
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      channel_indicator_utils::GetChannelNameStringResourceID(
          channel_, /*append_channel=*/true)));
}

void ChannelIndicatorView::SetImageOrText() {
  DCHECK(channel_indicator_utils::IsDisplayableChannel(channel_));

  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    // User is logged in, show the icon.
    if (image_view())
      return;

    DestroyLabel();
    CreateImageView();

    // Parent's border insets depend on shelf horizontal alignment. Note that
    // this modifies the circular background (created below), and can cause
    // clipping if incorrectly positioned/sized.
    SetBorder(views::CreateEmptyBorder(IsHorizontalAlignment()
                                           ? gfx::Insets::VH(kBorderInset, 0)
                                           : gfx::Insets::VH(0, kBorderInset)));
    image_view()->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(kIconBackgroundInset)));
    image_view()->SetBackground(views::CreateThemedRoundedRectBackground(
        channel_indicator_utils::GetBgColorJelly(channel_),
        (IsHorizontalAlignment() ? GetLocalBounds().width()
                                 : GetLocalBounds().height()) /
            2.0f));
    image_view()->SetImage(ui::ImageModel::FromVectorIcon(
        channel_indicator_utils::GetVectorIcon(channel_),
        channel_indicator_utils::GetFgColorJelly(channel_), kVectorIconSize));
    PreferredSizeChanged();
    return;
  }

  // User is not logged in, show the channel name.
  if (label())
    return;

  DestroyImageView();
  CreateLabel();

  // Label is only displayed if the user is in a non-active `SessionState`,
  // where side-shelf isn't possible (for now at least!), so nothing here is
  // adjusted for shelf alignment.
  DCHECK(IsHorizontalAlignment());
  box_layout_->set_inside_border_insets(gfx::Insets());
  SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(kBorderInset, 0)));
  label()->SetBorder(
      views::CreateEmptyBorder(gfx::Insets::VH(0, kBorderInset)));
  label()->SetBackground(views::CreateThemedRoundedRectBackground(
      channel_indicator_utils::GetBgColorJelly(channel_),
      kIndicatorBgCornerRadius));
  label()->SetEnabledColorId(
      channel_indicator_utils::GetFgColorJelly(channel_));

  label()->SetText(l10n_util::GetStringUTF16(
      channel_indicator_utils::GetChannelNameStringResourceID(
          channel_,
          /*append_channel=*/false)));
  label()->SetFontList(
      gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  PreferredSizeChanged();
}

void ChannelIndicatorView::OnAccessibleNameChanged(
    const std::u16string& new_name) {
  // If icon is showing, set it on the image view.
  if (image_view()) {
    DCHECK(!label());
    image_view()->GetViewAccessibility().SetName(new_name);
    return;
  }

  // Otherwise set it on the label.
  if (label())
    label()->GetViewAccessibility().SetName(new_name);
}

void ChannelIndicatorView::SetTooltip() {
  DCHECK(channel_indicator_utils::IsDisplayableChannel(channel_));
  tooltip_ = l10n_util::GetStringUTF16(
      channel_indicator_utils::GetChannelNameStringResourceID(
          channel_, /*append_channel=*/true));
}

void ChannelIndicatorView::OnSessionStateChanged(
    session_manager::SessionState state) {
  Update();
}

void ChannelIndicatorView::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  if (image_view()) {
    // Parent's border insets depend on shelf horizontal alignment. Note that
    // this modifies the circular background, and can cause clipping if
    // incorrectly positioned/sized.
    SetBorder(views::CreateEmptyBorder(IsHorizontalAlignment()
                                           ? gfx::Insets::VH(kBorderInset, 0)
                                           : gfx::Insets::VH(0, kBorderInset)));
  }
}

bool ChannelIndicatorView::IsLabelVisibleForTesting() {
  return label() && label()->GetVisible();
}

bool ChannelIndicatorView::IsImageViewVisibleForTesting() {
  return image_view() && image_view()->GetVisible();
}

std::u16string ChannelIndicatorView::GetAccessibleNameString() const {
  if (image_view())
    return image_view()->GetViewAccessibility().GetCachedName();

  if (label())
    return label()->GetViewAccessibility().GetCachedName();

  return std::u16string();
}

BEGIN_METADATA(ChannelIndicatorView)
END_METADATA

}  // namespace ash
