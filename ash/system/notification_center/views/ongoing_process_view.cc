// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/ongoing_process_view.h"

#include <string>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/system_shadow.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/message_center/message_center_impl.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr gfx::Insets kInteriorMargin = gfx::Insets::TLBR(12, 18, 12, 16);
constexpr gfx::Insets kIconAndLabelContainerDefaultMargins =
    gfx::Insets::VH(0, 16);
constexpr gfx::Insets kButtonsContainerDefaultMargins = gfx::Insets::VH(0, 6);
constexpr gfx::Insets kButtonsContainerInteriorMargin =
    gfx::Insets::TLBR(0, 12, 0, 0);

constexpr int kIconSize = 20;
constexpr int kTitleMaxLines = 2;
constexpr int kSubtitleMaxLines = 2;
constexpr int kSubtitleLineHeight = 18;

NotificationCatalogName GetCatalogName(
    const message_center::Notification& notification) {
  return notification.notifier_id().type ==
                 message_center::NotifierType::SYSTEM_COMPONENT
             ? notification.notifier_id().catalog_name
             : NotificationCatalogName::kNone;
}

}  // namespace

OngoingProcessView::OngoingProcessView(
    const message_center::Notification& notification)
    : MessageView(notification) {
  // `notification` must have a title and an icon. Will record the metrics
  // `ShownWithoutTitle` and `ShownWithoutIcon` to track outliers.
  auto catalog_name = GetCatalogName(notification);
  if (notification.title().empty()) {
    metrics_utils::LogOngoingProcessShownWithoutTitle(catalog_name);
  }
  if (notification.vector_small_image().is_empty()) {
    metrics_utils::LogOngoingProcessShownWithoutIcon(catalog_name);
  }

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(kInteriorMargin);

  auto* focus_ring = views::FocusRing::Get(this);
  focus_ring->SetColorId(ui::kColorAshFocusRing);
  focus_ring->SetProperty(views::kViewIgnoredByLayoutKey, true);

  auto* icon_and_label_container =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetIgnoreDefaultMainAxisMargins(true)
                       .SetCollapseMargins(true)
                       .Build());
  icon_and_label_container->SetDefault(views::kMarginsKey,
                                       kIconAndLabelContainerDefaultMargins);
  // Set `MaximumFlexSizeRule` to `kUnbounded` so the container expands and the
  // `buttons_container` is always on the trailing side of the notification.
  icon_and_label_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true)));

  icon_and_label_container->AddChildView(
      views::Builder<views::ImageView>()
          .SetID(VIEW_ID_ONGOING_PROCESS_ICON)
          .SetImage(ui::ImageModel::FromVectorIcon(
              notification.vector_small_image().is_empty()
                  ? message_center::kProductIcon
                  : notification.vector_small_image(),
              cros_tokens::kCrosSysOnSurface, kIconSize))
          .Build());

  auto* label_container = icon_and_label_container->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kVertical)
          .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
          .Build());
  // Set `MinimumFlexSizeRule` to `kScaleToZero` so labels can shrink and elide,
  // or wrap when multi-line text is enabled.
  label_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded,
                                   /*adjust_height_for_width=*/true)));

  label_container->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&title_label_)
          .SetID(VIEW_ID_ONGOING_PROCESS_TITLE_LABEL)
          .SetMultiLine(notification.message().empty())
          .SetMaxLines(kTitleMaxLines)
          .SetText(notification.title())
          .SetTooltipText(notification.title())
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosBody2))
          .Build());

  label_container->AddChildView(
      views::Builder<views::Label>()
          .CopyAddressTo(&subtitle_label_)
          .SetID(VIEW_ID_ONGOING_PROCESS_SUBTITLE_LABEL)
          .SetVisible(!notification.message().empty())
          .SetMultiLine(true)
          .SetMaxLines(kSubtitleMaxLines)
          .SetText(notification.message())
          .SetTooltipText(notification.message())
          .SetHorizontalAlignment(gfx::ALIGN_LEFT)
          .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
          .SetAutoColorReadabilityEnabled(false)
          .SetSubpixelRenderingEnabled(false)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation1))
          .SetLineHeight(kSubtitleLineHeight)
          .Build());

  auto* buttons_container =
      AddChildView(views::Builder<views::FlexLayoutView>()
                       .SetInteriorMargin(kButtonsContainerInteriorMargin)
                       .SetIgnoreDefaultMainAxisMargins(true)
                       .SetCollapseMargins(true)
                       .Build());
  buttons_container->SetDefault(views::kMarginsKey,
                                kButtonsContainerDefaultMargins);

  const bool has_pill_button = !notification.buttons().empty() &&
                               !notification.buttons()[0].title.empty();
  const bool has_icon_button =
      !has_pill_button && !notification.buttons().empty() &&
      !notification.buttons()[0].vector_icon->is_empty();

  if (has_pill_button) {
    const std::u16string& pill_button_title = notification.buttons()[0].title;

    buttons_container->AddChildView(
        views::Builder<PillButton>()
            .CopyAddressTo(&primary_pill_button_)
            .SetID(VIEW_ID_ONGOING_PROCESS_PILL_BUTTON)
            .SetText(pill_button_title)
            .SetTooltipText(pill_button_title)
            .SetPillButtonType(PillButton::Type::kPrimaryWithoutIcon)
            .SetCallback(base::BindRepeating(
                [](const std::string notification_id, int button_index) {
                  message_center::MessageCenter::Get()
                      ->ClickOnNotificationButton(notification_id,
                                                  button_index);
                },
                notification.id(), 0))
            .Build());
  } else if (has_icon_button) {
    int primary_index;
    message_center::ButtonInfo primary_button;

    // Check if there is a secondary icon button in the `buttons` list. Any
    // additional provided buttons will be ignored.
    if (notification.buttons().size() > 1 &&
        !notification.buttons()[1].vector_icon->is_empty()) {
      primary_index = 1;
      primary_button = notification.buttons()[primary_index];

      secondary_button_ = buttons_container->AddChildView(
          IconButton::Builder()
              .SetViewId(VIEW_ID_ONGOING_PROCESS_SECONDARY_ICON_BUTTON)
              .SetType(IconButton::Type::kMedium)
              .SetBackgroundColor(cros_tokens::kCrosSysSystemOnBase1)
              .SetVectorIcon(notification.buttons()[0].vector_icon)
              .SetAccessibleName(notification.buttons()[0].accessible_name)
              .SetCallback(base::BindRepeating(
                  [](const std::string notification_id, int button_index) {
                    message_center::MessageCenter::Get()
                        ->ClickOnNotificationButton(notification_id,
                                                    button_index);
                  },
                  notification.id(), 0))
              .Build());
    } else {
      primary_index = 0;
      primary_button = notification.buttons()[primary_index];
    }

    primary_icon_button_ = buttons_container->AddChildView(
        IconButton::Builder()
            .SetViewId(VIEW_ID_ONGOING_PROCESS_PRIMARY_ICON_BUTTON)
            .SetType(IconButton::Type::kMedium)
            .SetBackgroundColor(cros_tokens::kCrosSysHighlightShape)
            .SetVectorIcon(primary_button.vector_icon)
            .SetAccessibleName(primary_button.accessible_name)
            .SetCallback(base::BindRepeating(
                [](const std::string notification_id, int button_index) {
                  message_center::MessageCenter::Get()
                      ->ClickOnNotificationButton(notification_id,
                                                  button_index);
                },
                notification.id(), primary_index))
            .Build());
  }
}

OngoingProcessView::~OngoingProcessView() = default;

void OngoingProcessView::OnFocus() {
  MessageView::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void OngoingProcessView::OnThemeChanged() {
  // TODO(b/325129366): Land a config in `MessageView` that states if a
  // background should be painted, so there's no need to override
  // `OnThemeChanged` to prevent it from painting a background.
  views::View::OnThemeChanged();
}

void OngoingProcessView::UpdateWithNotification(
    const message_center::Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  // Only the `title` and `subtitle` labels and the label or icon inside of the
  // buttons can be updated. Any other changes will be ignored.
  if (title_label_ && title_label_->GetText() != notification.title()) {
    auto catalog_name = GetCatalogName(notification);
    if (notification.title().empty()) {
      metrics_utils::LogOngoingProcessShownWithoutTitle(catalog_name);
    }
    title_label_->SetText(notification.title());
  }

  if (subtitle_label_ && subtitle_label_->GetText() != notification.message()) {
    if (notification.message().empty()) {
      subtitle_label_->SetVisible(false);
    }
    if (subtitle_label_->GetText().empty()) {
      subtitle_label_->SetVisible(true);
    }
    subtitle_label_->SetText(notification.message());
  }

  // Return early if there were no buttons in the original notification.
  if (!primary_pill_button_ && !primary_icon_button_) {
    return;
  }

  // Buttons can't be added or removed, only their contents updated.
  const bool has_pill_button = primary_pill_button_ &&
                               !notification.buttons().empty() &&
                               !notification.buttons()[0].title.empty();
  const bool has_icon_button =
      primary_icon_button_ && !has_pill_button &&
      !notification.buttons().empty() &&
      !notification.buttons()[0].vector_icon->is_empty();

  if (has_pill_button) {
    primary_pill_button_->SetText(notification.buttons()[0].title);
    return;
  }

  if (has_icon_button) {
    int primary_button_index;
    if (secondary_button_ && notification.buttons().size() > 1 &&
        !notification.buttons()[1].vector_icon->is_empty()) {
      secondary_button_->SetVectorIcon(*notification.buttons()[0].vector_icon);
      primary_button_index = 1;
    } else {
      primary_button_index = 0;
    }

    primary_icon_button_->SetVectorIcon(
        *notification.buttons()[primary_button_index].vector_icon);
  }
}

message_center::NotificationControlButtonsView*
OngoingProcessView::GetControlButtonsView() const {
  return nullptr;
}

void OngoingProcessView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  views::FocusRing::Get(this)->InvalidateLayout();
}

BEGIN_METADATA(OngoingProcessView)
END_METADATA

}  // namespace ash
