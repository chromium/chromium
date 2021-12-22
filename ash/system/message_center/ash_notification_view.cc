// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/ash_notification_view.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/system/message_center/ash_notification_expand_button.h"
#include "ash/system/message_center/ash_notification_input_container.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/message_center/views/relative_time_formatter.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr gfx::Insets kNotificationViewPadding(0, 16, 18, 6);
constexpr gfx::Insets kMainRightViewPadding(0, 0, 0, 10);
constexpr int kMainRightViewVerticalSpacing = 4;

// This padding is applied to all the children of `main_right_view_` except the
// action buttons.
constexpr gfx::Insets kMainRightViewChildPadding(0, 14, 0, 0);

constexpr gfx::Insets kImageContainerPadding(12, 0, 0, 0);

constexpr gfx::Insets kActionButtonsRowPadding(16, 22, 0, 4);
constexpr int kActionsRowHorizontalSpacing = 8;

constexpr int kContentRowHorizontalSpacing = 16;
constexpr int kLeftContentVerticalSpacing = 4;
constexpr int kTitleRowSpacing = 6;

// Bullet character. The divider symbol between the title and the timestamp.
constexpr char16_t kTitleRowDivider[] = u"\u2022";

constexpr char kGoogleSansFont[] = "Google Sans";

constexpr int kAppIconViewSize = 24;
constexpr int kAppIconImageSize = 16;
constexpr int kTitleCharacterLimit =
    message_center::kNotificationWidth * message_center::kMaxTitleLines /
    message_center::kMinPixelsPerTitleCharacter;
constexpr int kTitleLabelSize = 14;
constexpr int kTimestampInCollapsedViewSize = 12;
constexpr int kMessageLabelSize = 13;
// The size for `icon_view_`, which is the icon within right content (between
// title/message view and expand button).
constexpr int kIconViewSize = 48;

// Lightness value that is used to calculate themed color used for app icon.
constexpr double kAppIconLightnessInDarkMode = 0.85;
constexpr double kAppIconLightnessInLightMode = 0.4;

// Helpers ---------------------------------------------------------------------

// Configure the style for labels in notification view. `is_color_primary`
// indicates if the color of the text is primary or secondary text color.
void ConfigureLabelStyle(
    views::Label* label,
    int size,
    bool is_color_primary,
    gfx::Font::Weight font_weight = gfx::Font::Weight::NORMAL) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(
      gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, size, font_weight));
  auto layer_type =
      is_color_primary
          ? ash::AshColorProvider::ContentLayerType::kTextColorPrimary
          : ash::AshColorProvider::ContentLayerType::kTextColorSecondary;
  label->SetEnabledColor(
      ash::AshColorProvider::Get()->GetContentLayerColor(layer_type));
}

// Create a view that will contain the `content_row`,
// `message_view_in_expanded_state_`, inline settings and the large image.
views::Builder<views::View> CreateMainRightViewBuilder() {
  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets(0, 0, kMainRightViewVerticalSpacing, 0))
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(kMainRightViewPadding);

  return views::Builder<views::View>()
      .SetID(message_center::NotificationViewBase::ViewId::kMainRightView)
      .SetLayoutManager(std::move(layout_manager))
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded));
}

// Create a view containing the title and message for the notification in a
// single line. This is used when a grouped child notification is in a
// collapsed parent notification.
views::Builder<views::BoxLayoutView> CreateCollapsedSummaryBuilder(
    const message_center::Notification& notification) {
  return views::Builder<views::BoxLayoutView>()
      .SetID(
          message_center::NotificationViewBase::ViewId::kCollapsedSummaryView)
      .SetInsideBorderInsets(ash::kGroupedCollapsedSummaryInsets)
      .SetBetweenChildSpacing(ash::kGroupedCollapsedSummaryLabelSpacing)
      .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
      .SetVisible(false)
      .AddChild(views::Builder<views::Label>()
                    .SetText(notification.title())
                    .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT))
      .AddChild(views::Builder<views::Label>()
                    .SetText(notification.message())
                    .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                    .SetTextStyle(views::style::STYLE_SECONDARY));
}

}  // namespace

namespace ash {

using CrossAxisAlignment = views::BoxLayout::CrossAxisAlignment;
using MainAxisAlignment = views::BoxLayout::MainAxisAlignment;
using Orientation = views::BoxLayout::Orientation;

BEGIN_METADATA(AshNotificationView, NotificationTitleRow, views::View)
END_METADATA

AshNotificationView::NotificationTitleRow::NotificationTitleRow(
    const std::u16string& title)
    : title_view_(AddChildView(GenerateTitleView(title))),
      title_row_divider_(AddChildView(std::make_unique<views::Label>(
          kTitleRowDivider,
          views::style::CONTEXT_DIALOG_BODY_TEXT))),
      timestamp_in_collapsed_view_(
          AddChildView(std::make_unique<views::Label>())) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetDefault(views::kMarginsKey, gfx::Insets(0, 0, 0, kTitleRowSpacing));
  title_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));

  ConfigureLabelStyle(title_row_divider_, kTimestampInCollapsedViewSize,
                      /*is_color_primary=*/false);
  message_center_utils::InitLayerForAnimations(title_row_divider_);
  ConfigureLabelStyle(timestamp_in_collapsed_view_,
                      kTimestampInCollapsedViewSize,
                      /*is_color_primary=*/false);
  message_center_utils::InitLayerForAnimations(timestamp_in_collapsed_view_);
  ConfigureLabelStyle(title_view_, kTitleLabelSize,
                      /*is_color_primary=*/true, gfx::Font::Weight::MEDIUM);
}

AshNotificationView::NotificationTitleRow::~NotificationTitleRow() {
  timestamp_update_timer_.Stop();
}

void AshNotificationView::NotificationTitleRow::UpdateTitle(
    const std::u16string& title) {
  title_view_->SetText(title);
}

void AshNotificationView::NotificationTitleRow::UpdateTimestamp(
    base::Time timestamp) {
  std::u16string relative_time;
  base::TimeDelta next_update;
  message_center::GetRelativeTimeStringAndNextUpdateTime(
      timestamp - base::Time::Now(), &relative_time, &next_update);

  timestamp_ = timestamp;
  timestamp_in_collapsed_view_->SetText(relative_time);

  // Unretained is safe as the timer cancels the task on destruction.
  timestamp_update_timer_.Start(
      FROM_HERE, next_update,
      base::BindOnce(&NotificationTitleRow::UpdateTimestamp,
                     base::Unretained(this), timestamp));
}

void AshNotificationView::NotificationTitleRow::UpdateVisibility(
    bool in_collapsed_mode) {
  timestamp_in_collapsed_view_->SetVisible(in_collapsed_mode);
  title_row_divider_->SetVisible(in_collapsed_mode);
}

void AshNotificationView::NotificationTitleRow::
    PerformExpandCollapseAnimation() {
  if (title_row_divider_->GetVisible()) {
    message_center_utils::FadeInView(
        title_row_divider_, kTitleRowTimestampFadeInAnimationDelayMs,
        kTitleRowTimestampFadeInAnimationDurationMs,
        gfx::Tween::ACCEL_20_DECEL_100);
    DCHECK(timestamp_in_collapsed_view_->GetVisible());
    message_center_utils::FadeInView(
        timestamp_in_collapsed_view_, kTitleRowTimestampFadeInAnimationDelayMs,
        kTitleRowTimestampFadeInAnimationDurationMs,
        gfx::Tween::ACCEL_20_DECEL_100);
  }
}

// static
const char AshNotificationView::kViewClassName[] = "AshNotificationView";

AshNotificationView::AshNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup)
    : NotificationViewBase(notification), shown_in_popup_(shown_in_popup) {
  // TODO(crbug/1232197): fix views and layout to match spec.
  // Instantiate view instances and define layout and view hierarchy.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(notification.group_child() ? gfx::Insets()
                                                    : kNotificationViewPadding);

  auto content_row_layout = std::make_unique<views::FlexLayout>();
  content_row_layout->SetInteriorMargin(kMainRightViewChildPadding);

  auto content_row_builder =
      CreateContentRowBuilder()
          .SetLayoutManager(std::move(content_row_layout))
          .AddChild(
              views::Builder<views::BoxLayoutView>()
                  .SetID(kHeaderLeftContent)
                  .SetOrientation(Orientation::kVertical)
                  .SetBetweenChildSpacing(kLeftContentVerticalSpacing)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kScaleToMaximum))
                  .AddChild(
                      CreateHeaderRowBuilder()
                          .SetIsInAshNotificationView(true)
                          .SetColor(
                              AshColorProvider::Get()->GetContentLayerColor(
                                  AshColorProvider::ContentLayerType::
                                      kTextColorSecondary)))
                  .AddChild(
                      CreateLeftContentBuilder()
                          .CopyAddressTo(&left_content_)
                          .SetBetweenChildSpacing(kLeftContentVerticalSpacing)))
          .AddChild(
              views::Builder<views::BoxLayoutView>()
                  .SetMainAxisAlignment(MainAxisAlignment::kEnd)
                  .SetInsideBorderInsets(
                      gfx::Insets(0, kContentRowHorizontalSpacing, 0, 0))
                  .SetBetweenChildSpacing(kContentRowHorizontalSpacing)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded))
                  .AddChild(CreateRightContentBuilder())
                  .AddChild(
                      views::Builder<views::FlexLayoutView>()
                          .CopyAddressTo(&expand_button_container_)
                          .SetOrientation(views::LayoutOrientation::kHorizontal)
                          .AddChild(
                              views::Builder<AshNotificationExpandButton>()
                                  .CopyAddressTo(&expand_button_)
                                  .SetCallback(base::BindRepeating(
                                      &AshNotificationView::ToggleExpand,
                                      base::Unretained(this)))
                                  .SetProperty(
                                      views::kCrossAxisAlignmentKey,
                                      views::LayoutAlignment::kStart))));

  // Main right view contains all the views besides control buttons and
  // icon.
  auto main_right_view_builder =
      CreateMainRightViewBuilder()
          .AddChild(content_row_builder)
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&message_view_in_expanded_state_)
                  .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                  .SetMultiLine(true)
                  .SetMaxLines(message_center::kMaxLinesForExpandedMessageView)
                  .SetAllowCharacterBreak(true)
                  .SetBorder(
                      views::CreateEmptyBorder(kMainRightViewChildPadding))
                  // TODO(crbug/682266): This is a workaround to that bug by
                  // explicitly setting the width. Ideally, we should fix the
                  // original bug, but it seems there's no obvious solution for
                  // the bug according to https://crbug.com/678337#c7. We will
                  // consider making changes to this code when the bug is fixed.
                  .SetMaximumWidth(GetExpandedMessageViewWidth()))
          .AddChild(CreateInlineSettingsBuilder())
          .AddChild(CreateImageContainerBuilder().SetBorder(
              views::CreateEmptyBorder(kImageContainerPadding)));

  ConfigureLabelStyle(message_view_in_expanded_state_, kMessageLabelSize,
                      false);

  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&control_buttons_container_)
          .SetInsideBorderInsets(IsExpanded()
                                     ? kControlButtonsContainerExpandedPadding
                                     : kControlButtonsContainerCollapsedPadding)
          .SetMainAxisAlignment(MainAxisAlignment::kEnd)
          .SetVisible(!notification.group_child())
          .AddChild(CreateControlButtonsBuilder()
                        .CopyAddressTo(&control_buttons_view_)
                        .SetButtonIconColors(
                            AshColorProvider::Get()->GetContentLayerColor(
                                AshColorProvider::ContentLayerType::
                                    kIconColorPrimary)))
          .Build());

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .CopyAddressTo(&main_view_)
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .AddChild(views::Builder<views::BoxLayoutView>()
                        .SetID(kAppIconViewContainer)
                        .SetOrientation(Orientation::kVertical)
                        .SetMainAxisAlignment(MainAxisAlignment::kStart)
                        .AddChild(views::Builder<RoundedImageView>()
                                      .CopyAddressTo(&app_icon_view_)
                                      .SetCornerRadius(kAppIconViewSize / 2)))
          .AddChild(main_right_view_builder)
          .Build());

  AddChildView(CreateCollapsedSummaryBuilder(notification)
                   .CopyAddressTo(&collapsed_summary_view_)
                   .Build());

  AddChildView(views::Builder<views::BoxLayoutView>()
                   .CopyAddressTo(&grouped_notifications_container_)
                   .SetOrientation(Orientation::kVertical)
                   .SetInsideBorderInsets(kGroupedNotificationContainerInsets)
                   .SetBetweenChildSpacing(
                       IsExpanded() ? kGroupedNotificationsExpandedSpacing
                                    : kGroupedNotificationsCollapsedSpacing)
                   .Build());

  AddChildView(CreateActionsRow(std::make_unique<views::FlexLayout>()));

  // Custom layout and paddings for views in `AshNotificationView`.
  action_buttons_row()
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets(0, 0, 0, kActionsRowHorizontalSpacing))
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetInteriorMargin(kActionButtonsRowPadding);
  action_buttons_row()->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  inline_reply()->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  static_cast<views::FlexLayout*>(header_row()->GetLayoutManager())
      ->SetDefault(views::kMarginsKey, gfx::Insets())
      .SetInteriorMargin(gfx::Insets());
  header_row()->ConfigureLabelsStyle(
      gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, kHeaderViewLabelSize,
                    gfx::Font::Weight::NORMAL),
      gfx::Insets(), true);

  if (shown_in_popup_ && !notification.group_child()) {
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{kMessagePopupCornerRadius});
  } else if (!notification.group_child()) {
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF{kMessageCenterNotificationCornerRadius});
  }
  layer()->SetIsFastRoundedCorner(true);

  // Create layer in some views for animations.
  message_center_utils::InitLayerForAnimations(header_row());
  message_center_utils::InitLayerForAnimations(message_view_in_expanded_state_);
  message_center_utils::InitLayerForAnimations(actions_row());

  UpdateWithNotification(notification);
}

AshNotificationView::~AshNotificationView() = default;

void AshNotificationView::SetGroupedChildExpanded(bool expanded) {
  collapsed_summary_view_->SetVisible(!expanded);
  main_view_->SetVisible(expanded);
  control_buttons_container_->SetVisible(expanded);
}

void AshNotificationView::AnimateGroupedChildExpandedCollapse(bool expanded) {
  message_center_utils::InitLayerForAnimations(collapsed_summary_view_);
  message_center_utils::InitLayerForAnimations(main_view_);
  // Fade out `collapsed_summary_view_`, then fade in `main_view_` in expanded
  // state and vice versa in collapsed state.
  if (expanded) {
    message_center_utils::FadeOutView(
        collapsed_summary_view_,
        base::BindRepeating(
            [](base::WeakPtr<ash::AshNotificationView> parent,
               views::View* collapsed_summary_view) {
              if (parent) {
                collapsed_summary_view->layer()->SetOpacity(1.0f);
                collapsed_summary_view->SetVisible(false);
              }
            },
            weak_factory_.GetWeakPtr(), collapsed_summary_view_),
        0, kCollapsedSummaryViewAnimationDurationMs);
    message_center_utils::FadeInView(main_view_,
                                     kCollapsedSummaryViewAnimationDurationMs,
                                     kChildMainViewFadeInAnimationDurationMs);
    return;
  }

  message_center_utils::FadeOutView(
      main_view_,
      base::BindRepeating(
          [](base::WeakPtr<ash::AshNotificationView> parent,
             views::View* main_view) {
            if (parent) {
              main_view->layer()->SetOpacity(1.0f);
              main_view->SetVisible(false);
            }
          },
          weak_factory_.GetWeakPtr(), main_view_),
      0, kChildMainViewFadeOutAnimationDurationMs);
  message_center_utils::FadeInView(collapsed_summary_view_,
                                   kChildMainViewFadeOutAnimationDurationMs,
                                   kCollapsedSummaryViewAnimationDurationMs);
}

void AshNotificationView::ToggleExpand() {
  SetManuallyExpandedOrCollapsed(true);

  if (inline_reply()->GetVisible()) {
    // If inline reply is visible, fade it out then set expanded.
    message_center_utils::FadeOutView(
        inline_reply(),
        base::BindOnce(
            [](base::WeakPtr<ash::AshNotificationView> parent,
               views::View* inline_reply) {
              if (parent) {
                inline_reply->layer()->SetOpacity(1.0f);
              }
            },
            weak_factory_.GetWeakPtr(), inline_reply()),
        /*delay_in_ms=*/0, kInlineReplyFadeOutAnimationDurationMs);
  }

  SetExpanded(!IsExpanded());

  PerformExpandCollapseAnimation();
}

base::TimeDelta AshNotificationView::GetBoundsAnimationDuration(
    const message_center::Notification& notification) const {
  // This is called after the parent gets notified of
  // `ChildPreferredSizeChanged()`, so the current expanded state is the target
  // state.
  if (!notification.image().IsEmpty())
    return base::Milliseconds(kLargeImageExpandAndCollapseAnimationDuration);

  if (HasInlineReply(notification) || is_grouped_parent_view_) {
    if (IsExpanded()) {
      return base::Milliseconds(
          kInlineReplyAndGroupedParentExpandAnimationDuration);
    }
    return base::Milliseconds(
        kInlineReplyAndGroupedParentCollapseAnimationDuration);
  }

  if (inline_settings_row() && inline_settings_row()->GetVisible()) {
    return base::Milliseconds(
        kInlineSettingsExpandAndCollapseAnimationDuration);
  }

  if (IsExpanded())
    return base::Milliseconds(kGeneralExpandAnimationDuration);
  return base::Milliseconds(kGeneralCollapseAnimationDuration);
}

void AshNotificationView::AddGroupNotification(
    const message_center::Notification& notification,
    bool newest_first) {
  auto notification_view =
      std::make_unique<AshNotificationView>(notification,
                                            /*shown_in_popup=*/false);
  notification_view->SetVisible(
      total_grouped_notifications_ <
          message_center_style::kMaxGroupedNotificationsInCollapsedState ||
      IsExpanded());
  notification_view->SetGroupedChildExpanded(IsExpanded());
  grouped_notifications_container_->AddChildViewAt(
      std::move(notification_view),
      newest_first ? 0 : grouped_notifications_container_->children().size());

  total_grouped_notifications_++;
  left_content_->SetVisible(false);
  expand_button_->UpdateGroupedNotificationsCount(total_grouped_notifications_);
  PreferredSizeChanged();
}

void AshNotificationView::PopulateGroupNotifications(
    const std::vector<const message_center::Notification*>& notifications) {
  DCHECK(total_grouped_notifications_ == 0);
  for (auto* notification : notifications) {
    auto notification_view =
        std::make_unique<AshNotificationView>(*notification,
                                              /*shown_in_popup=*/false);
    notification_view->SetVisible(
        total_grouped_notifications_ <
            message_center_style::kMaxGroupedNotificationsInCollapsedState ||
        IsExpanded());
    notification_view->SetGroupedChildExpanded(IsExpanded());
    grouped_notifications_container_->AddChildViewAt(
        std::move(notification_view), 0);
  }
  total_grouped_notifications_ = notifications.size();
  left_content_->SetVisible(total_grouped_notifications_ == 0);
  expand_button_->UpdateGroupedNotificationsCount(total_grouped_notifications_);
}

void AshNotificationView::RemoveGroupNotification(
    const std::string& notification_id) {
  AshNotificationView* to_be_deleted = nullptr;
  for (auto* child : grouped_notifications_container_->children()) {
    AshNotificationView* group_notification =
        static_cast<AshNotificationView*>(child);
    if (group_notification->notification_id() == notification_id) {
      to_be_deleted = group_notification;
      break;
    }
  }
  if (to_be_deleted)
    delete to_be_deleted;

  total_grouped_notifications_--;
  left_content_->SetVisible(total_grouped_notifications_ == 0);
  expand_button_->UpdateGroupedNotificationsCount(total_grouped_notifications_);
  PreferredSizeChanged();
}

const char* AshNotificationView::GetClassName() const {
  return kViewClassName;
}

void AshNotificationView::UpdateViewForExpandedState(bool expanded) {
  static_cast<views::BoxLayout*>(control_buttons_container_->GetLayoutManager())
      ->set_inside_border_insets(
          expanded ? kControlButtonsContainerExpandedPadding
                   : kControlButtonsContainerCollapsedPadding);

  bool is_single_expanded_notification =
      !is_grouped_child_view_ && !is_grouped_parent_view_ && expanded;
  header_row()->SetVisible(is_grouped_parent_view_ ||
                           (is_single_expanded_notification));

  if (title_row_) {
    title_row_->UpdateVisibility(is_grouped_child_view_ ||
                                 (IsExpandable() && !expanded));
  }

  if (message_view()) {
    // `message_view()` is shown only in collapsed mode.
    if (!expanded) {
      ConfigureLabelStyle(message_view(), kMessageLabelSize, false);
      message_center_utils::InitLayerForAnimations(message_view());
    }
    message_view()->SetVisible(!expanded);
    message_view_in_expanded_state_->SetVisible(expanded &&
                                                !is_grouped_parent_view_);
  }

  // Custom padding for app icon and expand button. These 2 views should always
  // use the same padding value so that they are vertical aligned.
  app_icon_view_->SetBorder(views::CreateEmptyBorder(
      expanded ? kAppIconExpandButtonExpandedPadding
               : kAppIconExpandButtonCollapsedPadding));
  expand_button_container_->SetInteriorMargin(
      expanded ? kAppIconExpandButtonExpandedPadding
               : kAppIconExpandButtonCollapsedPadding);

  expand_button_->SetExpanded(expanded);

  static_cast<views::BoxLayout*>(
      grouped_notifications_container_->GetLayoutManager())
      ->set_between_child_spacing(expanded
                                      ? kGroupedNotificationsExpandedSpacing
                                      : kGroupedNotificationsCollapsedSpacing);

  int notification_count = 0;
  for (auto* child : grouped_notifications_container_->children()) {
    auto* notification_view = static_cast<AshNotificationView*>(child);
    notification_view->AnimateGroupedChildExpandedCollapse(expanded);
    notification_view->SetGroupedChildExpanded(expanded);
    notification_count++;
    if (notification_count >
        message_center_style::kMaxGroupedNotificationsInCollapsedState) {
      notification_view->SetVisible(expanded);
    }
  }

  NotificationViewBase::UpdateViewForExpandedState(expanded);
}

void AshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  is_grouped_child_view_ = notification.group_child();
  is_grouped_parent_view_ = notification.group_parent();

  grouped_notifications_container_->SetVisible(is_grouped_parent_view_);
  header_row()->SetVisible(!is_grouped_child_view_);
  UpdateMessageViewInExpandedState(notification);

  NotificationViewBase::UpdateWithNotification(notification);

  CreateOrUpdateSnoozeButton(notification);
}

void AshNotificationView::CreateOrUpdateHeaderView(
    const message_center::Notification& notification) {
  switch (notification.system_notification_warning_level()) {
    case message_center::SystemNotificationWarningLevel::WARNING:
      header_row()->SetSummaryText(
          l10n_util::GetStringUTF16(IDS_ASH_NOTIFICATION_WARNING_LABEL));
      break;
    case message_center::SystemNotificationWarningLevel::CRITICAL_WARNING:
      header_row()->SetSummaryText(l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_CRITICAL_WARNING_LABEL));
      break;
    case message_center::SystemNotificationWarningLevel::NORMAL:
      header_row()->SetSummaryText(std::u16string());
      break;
  }

  NotificationViewBase::CreateOrUpdateHeaderView(notification);
}

void AshNotificationView::CreateOrUpdateTitleView(
    const message_center::Notification& notification) {
  if (notification.title().empty()) {
    if (title_row_) {
      DCHECK(left_content()->Contains(title_row_));
      left_content()->RemoveChildViewT(title_row_);
      title_row_ = nullptr;
    }
    return;
  }

  const std::u16string& title = gfx::TruncateString(
      notification.title(), kTitleCharacterLimit, gfx::WORD_BREAK);

  if (!title_row_) {
    title_row_ =
        AddViewToLeftContent(std::make_unique<NotificationTitleRow>(title));
  } else {
    title_row_->UpdateTitle(title);
    ReorderViewInLeftContent(title_row_);
  }

  title_row_->UpdateTimestamp(notification.timestamp());
}

void AshNotificationView::CreateOrUpdateSmallIconView(
    const message_center::Notification& notification) {
  if (is_grouped_child_view_ && !notification.icon().IsEmpty()) {
    app_icon_view_->SetImage(notification.icon().AsImageSkia(),
                             gfx::Size(kAppIconViewSize, kAppIconViewSize));
    return;
  }

  UpdateAppIconView();
}

void AshNotificationView::CreateOrUpdateInlineSettingsViews(
    const message_center::Notification& notification) {
  if (inline_settings_enabled()) {
    // TODO(crbug/1265636): Fix this logic when grouped parent notification has
    // inline settings.
    DCHECK(is_grouped_parent_view_ ||
           (message_center::SettingsButtonHandler::INLINE ==
            notification.rich_notification_data().settings_button_handler));
    return;
  }

  set_inline_settings_enabled(
      notification.rich_notification_data().settings_button_handler ==
      message_center::SettingsButtonHandler::INLINE);

  if (!inline_settings_enabled()) {
    return;
  }

  inline_settings_row()->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), 0));
  auto turn_off_notifications_button = GenerateNotificationLabelButton(
      base::BindRepeating(&AshNotificationView::DisableNotification,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_INLINE_SETTINGS_TURN_OFF_BUTTON_TEXT));
  turn_off_notifications_button_ = inline_settings_row()->AddChildView(
      std::move(turn_off_notifications_button));
  auto inline_settings_cancel_button = GenerateNotificationLabelButton(
      base::BindRepeating(&AshNotificationView::ToggleInlineSettings,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_INLINE_SETTINGS_CANCEL_BUTTON_TEXT));
  inline_settings_cancel_button_ = inline_settings_row()->AddChildView(
      std::move(inline_settings_cancel_button));
}

void AshNotificationView::UpdateControlButtonsVisibility() {
  NotificationViewBase::UpdateControlButtonsVisibility();

  // Always hide snooze button in control buttons since we show this snooze
  // button in actions button view.
  control_buttons_view()->ShowSnoozeButton(false);
}

bool AshNotificationView::IsIconViewShown() const {
  return NotificationViewBase::IsIconViewShown() && !is_grouped_child_view_;
}

void AshNotificationView::SetExpandButtonEnabled(bool enabled) {
  expand_button_->SetVisible(enabled);
}

bool AshNotificationView::IsExpandable() const {
  // Inline settings can not be expanded.
  if (GetMode() == Mode::SETTING)
    return false;

  // Notification should always be expandable since we hide `header_row()` in
  // collapsed state.
  return true;
}

void AshNotificationView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  // Call parent's SetCornerRadius to update radius used for highlight path.
  NotificationViewBase::SetCornerRadius(top_radius, bottom_radius);
  UpdateBackground(top_radius, bottom_radius);
}

void AshNotificationView::SetDrawBackgroundAsActive(bool active) {}

void AshNotificationView::OnThemeChanged() {
  views::View::OnThemeChanged();
  UpdateBackground(top_radius_, bottom_radius_);

  UpdateAppIconView();

  header_row()->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));

  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));
}

std::unique_ptr<message_center::NotificationInputContainer>
AshNotificationView::GenerateNotificationInputContainer() {
  return std::make_unique<AshNotificationInputContainer>(this);
}

std::unique_ptr<views::LabelButton>
AshNotificationView::GenerateNotificationLabelButton(
    views::Button::PressedCallback callback,
    const std::u16string& label) {
  std::unique_ptr<views::LabelButton> actions_button =
      std::make_unique<PillButton>(std::move(callback), label,
                                   PillButton::Type::kIconlessAccentFloating,
                                   /*icon=*/nullptr);
  // Override the inkdrop configuration to make sure it will show up when hover
  // or focus on the button.
  StyleUtil::SetUpInkDropForButton(actions_button.get(), gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
  return actions_button;
}

gfx::Size AshNotificationView::GetIconViewSize() const {
  return gfx::Size(kIconViewSize, kIconViewSize);
}

int AshNotificationView::GetLargeImageViewMaxWidth() const {
  return message_center::kNotificationWidth - kNotificationViewPadding.width() -
         kAppIconViewSize - kMainRightViewPadding.width() -
         kMainRightViewChildPadding.width();
}

void AshNotificationView::ToggleInlineSettings(const ui::Event& event) {
  if (!inline_settings_enabled())
    return;

  NotificationViewBase::ToggleInlineSettings(event);

  bool inline_settings_visible = inline_settings_row()->GetVisible();

  // In settings UI, we only show the app icon and header row along with the
  // inline settings UI.
  header_row()->SetVisible(true);
  left_content()->SetVisible(!inline_settings_visible);
  right_content()->SetVisible(!inline_settings_visible);
  expand_button_->SetVisible(!inline_settings_visible);
}

void AshNotificationView::ActionButtonPressed(size_t index,
                                              const ui::Event& event) {
  NotificationViewBase::ActionButtonPressed(index, event);

  // If inline reply is visible, fade out actions button and then fade in inline
  // reply.
  if (inline_reply()->GetVisible()) {
    message_center_utils::InitLayerForAnimations(action_buttons_row());
    message_center_utils::FadeOutView(
        action_buttons_row(),
        base::BindOnce(
            [](base::WeakPtr<ash::AshNotificationView> parent,
               views::View* action_buttons_row) {
              if (parent) {
                action_buttons_row->layer()->SetOpacity(1.0f);
                action_buttons_row->SetVisible(false);
              }
            },
            weak_factory_.GetWeakPtr(), action_buttons_row()),
        /*delay_in_ms=*/0, kActionButtonsFadeOutAnimationDurationMs);

    // Delay for the action buttons to fade out, then fade in inline reply.
    message_center_utils::InitLayerForAnimations(inline_reply());
    message_center_utils::FadeInView(inline_reply(),
                                     kActionButtonsFadeOutAnimationDurationMs,
                                     kInlineReplyFadeInAnimationDurationMs);
  }
}

void AshNotificationView::CreateOrUpdateSnoozeButton(
    const message_center::Notification& notification) {
  if (!notification.should_show_snooze_button()) {
    if (action_buttons_row()->Contains(snooze_button_)) {
      action_buttons_row()->RemoveChildViewT(snooze_button_);
      snooze_button_ = nullptr;
      DCHECK(action_buttons_row()->Contains(snooze_button_spacer_));
      action_buttons_row()->RemoveChildViewT(snooze_button_spacer_);
      snooze_button_spacer_ = nullptr;
    }
    return;
  }

  if (snooze_button_) {
    DCHECK(snooze_button_spacer_);
    // Spacer and snooze button should be at the end of action buttons row.
    action_buttons_row()->ReorderChildView(snooze_button_spacer_, -1);
    action_buttons_row()->ReorderChildView(snooze_button_, -1);
    return;
  }

  action_buttons_row()->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&snooze_button_spacer_)
          .SetMainAxisAlignment(MainAxisAlignment::kEnd)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());

  // TODO(crbug/1276581): Make sure the style of this snooze button is fit
  // with the style of other action buttons.
  action_buttons_row()->AddChildView(
      views::Builder<views::ImageButton>()
          .CopyAddressTo(&snooze_button_)
          .SetCallback(
              base::BindRepeating(&AshNotificationView::OnSnoozeButtonPressed,
                                  base::Unretained(this)))
          .SetTooltipText(l10n_util::GetStringUTF16(
              IDS_MESSAGE_CENTER_NOTIFICATION_SNOOZE_BUTTON_TOOLTIP))
          .Build());

  SkColor button_color = notification.accent_color().value_or(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive));
  views::SetImageFromVectorIcon(snooze_button_, kNotificationSnoozeButtonIcon,
                                20, button_color);
}

void AshNotificationView::UpdateMessageViewInExpandedState(
    const message_center::Notification& notification) {
  if (notification.message().empty()) {
    message_view_in_expanded_state_->SetVisible(false);
    return;
  }
  message_view_in_expanded_state_->SetText(gfx::TruncateString(
      notification.message(), message_center::kMessageCharacterLimit,
      gfx::WORD_BREAK));

  message_view_in_expanded_state_->SetVisible(true);
}

void AshNotificationView::UpdateBackground(int top_radius, int bottom_radius) {
  SkColor background_color;
  if (shown_in_popup_) {
    background_color = AshColorProvider::Get()->GetBaseLayerColor(
        AshColorProvider::BaseLayerType::kTransparent80);
  } else {
    background_color = AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  }

  if (background_color == background_color_ && top_radius_ == top_radius &&
      bottom_radius_ == bottom_radius) {
    return;
  }

  if (!is_grouped_child_view_)
    background_color_ = background_color;
  top_radius_ = top_radius;
  bottom_radius_ = bottom_radius;

  SetBackground(views::CreateBackgroundFromPainter(
      std::make_unique<message_center::NotificationBackgroundPainter>(
          top_radius_, bottom_radius_, background_color_)));
}

int AshNotificationView::GetExpandedMessageViewWidth() {
  int notification_width = shown_in_popup_ ? message_center::kNotificationWidth
                                           : kNotificationInMessageCenterWidth;

  return notification_width - kNotificationViewPadding.width() -
         kAppIconViewSize - kMainRightViewPadding.width() -
         kMainRightViewChildPadding.width();
}

void AshNotificationView::DisableNotification() {
  message_center::MessageCenter::Get()->DisableNotification(notification_id());
}

void AshNotificationView::UpdateAppIconView() {
  auto* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id());

  // Grouped child notification use notification's icon for the app icon view,
  // so we don't need further update here.
  if (!notification ||
      (is_grouped_child_view_ && !notification->icon().IsEmpty()))
    return;

  SkColor accent_color = notification->accent_color().value_or(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorActive));

  SkColor icon_color = AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kTransparent80);

  // To ensure the app icon looks distinct enough in the notification
  // background, we change the lightness of the accent color to generate app
  // icon's background color.
  color_utils::HSL hsl;
  color_utils::SkColorToHSL(accent_color, &hsl);
  hsl.l = AshColorProvider::Get()->IsDarkModeEnabled()
              ? kAppIconLightnessInDarkMode
              : kAppIconLightnessInLightMode;
  SkColor icon_background_color =
      color_utils::HSLToSkColor(hsl, SkColorGetA(accent_color));

  // TODO(crbug.com/768748): figure out if this has a performance impact and
  // cache images if so.
  gfx::Image masked_small_icon = notification->GenerateMaskedSmallIcon(
      kAppIconImageSize, icon_color, icon_background_color, icon_color);

  gfx::ImageSkia app_icon =
      masked_small_icon.IsEmpty()
          ? gfx::CreateVectorIcon(message_center::kProductIcon,
                                  kAppIconImageSize, icon_color)
          : masked_small_icon.AsImageSkia();

  app_icon_view_->SetImage(
      gfx::ImageSkiaOperations::CreateImageWithCircleBackground(
          kAppIconViewSize / 2, icon_background_color, app_icon));
}

void AshNotificationView::PerformExpandCollapseAnimation() {
  // Ensure layout is up-to-date before animating views.
  if (needs_layout())
    Layout();
  DCHECK(!needs_layout());

  if (title_row_)
    title_row_->PerformExpandCollapseAnimation();

  // Fade in `header row()` if this is not a grouped parent view.
  if (header_row()->GetVisible() && !is_grouped_parent_view_) {
    message_center_utils::FadeInView(header_row(),
                                     kHeaderRowFadeInAnimationDelayMs,
                                     kHeaderRowFadeInAnimationDurationMs);
  }

  // Fade in `message_view()`.
  if (message_view()) {
    message_center_utils::FadeInView(message_view(),
                                     kMessageViewFadeInAnimationDelayMs,
                                     kMessageViewFadeInAnimationDurationMs);
  }

  // Fade in `message_view_in_expanded_state_`.
  if (message_view_in_expanded_state_->GetVisible()) {
    message_center_utils::FadeInView(
        message_view_in_expanded_state_,
        kMessageViewInExpandedStateFadeInAnimationDelayMs,
        kMessageViewInExpandedStateFadeInAnimationDurationMs);
  }

  expand_button_->PerformExpandCollapseAnimation();

  if (actions_row()->GetVisible()) {
    message_center_utils::FadeInView(actions_row(),
                                     kActionsRowFadeInAnimationDelayMs,
                                     kActionsRowFadeInAnimationDurationMs);
  }
}

}  // namespace ash
