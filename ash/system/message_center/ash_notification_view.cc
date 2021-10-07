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
#include "ash/system/message_center/ash_notification_input_container.h"
#include "ash/system/message_center/message_center_constants.h"
#include "ash/system/message_center/message_center_style.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/check.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view.h"
#include "ui/message_center/views/relative_time_formatter.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

constexpr gfx::Insets kNotificationViewPadding(6, 16, 0, 6);
constexpr gfx::Insets kMainRightViewPadding(0, 0, 0, 10);
constexpr int kMainRightViewVerticalSpacing = 16;
constexpr gfx::Insets kContentRowPadding(0, 14, 0, 0);
constexpr int kContentRowHorizontalSpacing = 16;
constexpr int kLeftContentVerticalSpacing = 4;
constexpr int kTitleRowSpacing = 6;

// Bullet character. The divider symbol between the title and the timestamp.
constexpr char16_t kTitleRowDivider[] = u"\u2022";

constexpr char kGoogleSansFont[] = "Google Sans";
constexpr char kLabelButtonFontSize = 13;

constexpr int kAppIconViewSize = 24;
constexpr int kExpandButtonSize = 24;
constexpr int kTitleCharacterLimit =
    message_center::kNotificationWidth * message_center::kMaxTitleLines /
    message_center::kMinPixelsPerTitleCharacter;
constexpr int kExpandedTitleLabelSize = 16;
constexpr int kCollapsedTitleLabelSize = 14;
constexpr gfx::Insets kLabelButtonInsets(6, 16);
constexpr int kLabelButtonCornerRadius = 24;
constexpr int kTimestampInCollapsedViewSize = 12;
constexpr int kMessageLabelSize = 13;
// The size for `icon_view_`, which is the icon within right content (between
// title/message view and expand button).
constexpr int kIconViewSize = 48;

constexpr int kContentRowWidth =
    message_center::kNotificationWidth - kNotificationViewPadding.width() -
    kAppIconViewSize - kMainRightViewPadding.width() -
    kContentRowPadding.width();

constexpr int kLeftContentWidth = kContentRowWidth - kExpandButtonSize -
                                  kAppIconViewSize -
                                  kContentRowHorizontalSpacing * 2;

// Configure the style for labels in notification view. `is_color_primary`
// indicates if the color of the text is primary or secondary text color.
void ConfigureLabelStyle(views::Label* label, int size, bool is_color_primary) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, size,
                                   gfx::Font::Weight::MEDIUM));
  auto layer_type =
      is_color_primary
          ? ash::AshColorProvider::ContentLayerType::kTextColorPrimary
          : ash::AshColorProvider::ContentLayerType::kTextColorSecondary;
  label->SetEnabledColor(
      ash::AshColorProvider::Get()->GetContentLayerColor(layer_type));
}

// A class to provide custom (non MD) styling for buttons shown in the actions
// row. NotificationViewBase uses an MD button, both inherit from
// views::LabelButton.
class AshNotificationLabelButton : public views::LabelButton {
 public:
  AshNotificationLabelButton(PressedCallback callback,
                             const std::u16string& text)
      : views::LabelButton(std::move(callback),
                           text,
                           views::style::CONTEXT_BUTTON_MD) {
    // TODO(crbug/1255172): Convert this "pill button no icon floating" to use
    // the element_style when it is implemented.
    const SkColor enabled_text_color =
        ash::AshColorProvider::Get()->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kButtonLabelColorBlue);

    // TODO(crbug/1255159): Consider setting the disabled color as well.
    SetEnabledTextColors(enabled_text_color);
    label()->SetAutoColorReadabilityEnabled(true);

    SetBorder(views::CreateEmptyBorder(kLabelButtonInsets));
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kLabelButtonCornerRadius);
    views::FocusRing::Get(this)->SetColor(
        ash::AshColorProvider::Get()->GetControlsLayerColor(
            ash::AshColorProvider::ControlsLayerType::kFocusRingColor));
  }
  AshNotificationLabelButton(const AshNotificationLabelButton&) = delete;
  AshNotificationLabelButton& operator=(const AshNotificationLabelButton&) =
      delete;
  ~AshNotificationLabelButton() override = default;

 private:
  // views::LabelButton:
  views::PropertyEffects UpdateStyleToIndicateDefaultStatus() override {
    label()->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                       kLabelButtonFontSize,
                                       gfx::Font::Weight::MEDIUM));
    return views::kPropertyEffectsPreferredSizeChanged;
  }

  void OnThemeChanged() override {
    views::LabelButton::OnThemeChanged();
    // TODO(crbug/1255172): Convert this "pill button no icon floating" to use
    // the element_style when it is implemented.
    const SkColor enabled_text_color =
        ash::AshColorProvider::Get()->GetContentLayerColor(
            ash::AshColorProvider::ContentLayerType::kButtonLabelColorBlue);

    // TODO(crbug/1255159): Consider setting the disabled color as well.
    SetEnabledTextColors(enabled_text_color);
    views::FocusRing::Get(this)->SetColor(
        ash::AshColorProvider::Get()->GetControlsLayerColor(
            ash::AshColorProvider::ControlsLayerType::kFocusRingColor));
  }
};

}  // namespace

namespace ash {

using Orientation = views::BoxLayout::Orientation;

BEGIN_METADATA(AshNotificationView, NotificationTitleRow, views::View)
END_METADATA
BEGIN_METADATA(AshNotificationView, ExpandButton, views::Button)
END_METADATA

AshNotificationView::NotificationTitleRow::NotificationTitleRow(
    const std::u16string& title)
    : title_view_(AddChildView(GenerateTitleView(title))),
      title_row_divider_(AddChildView(std::make_unique<views::Label>(
          kTitleRowDivider,
          views::style::CONTEXT_DIALOG_BODY_TEXT))),
      timestamp_in_collapsed_view_(
          AddChildView(std::make_unique<views::Label>())) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kHorizontal, gfx::Insets(), kTitleRowSpacing));
  ConfigureLabelStyle(title_row_divider_, kTimestampInCollapsedViewSize,
                      /*is_color_primary=*/false);
  ConfigureLabelStyle(timestamp_in_collapsed_view_,
                      kTimestampInCollapsedViewSize,
                      /*is_color_primary=*/false);
  ConfigureLabelStyle(title_view_, kExpandedTitleLabelSize,
                      /*is_color_primary=*/true);
}

AshNotificationView::NotificationTitleRow::~NotificationTitleRow() {
  timestamp_update_timer_.Stop();
}

void AshNotificationView::NotificationTitleRow::SetExpanded(bool expanded) {
  ConfigureLabelStyle(
      title_view_,
      expanded ? kExpandedTitleLabelSize : kCollapsedTitleLabelSize,
      /*is_color_primary=*/true);

  // In expanded state, we need to resize the title view since it becomes
  // bigger. We also need to reset the fixed width in this label to zero in
  // collapsed state for the timestamp alongside it can be shown.
  title_view_->SizeToFit(expanded ? kContentRowWidth : 0);
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

AshNotificationView::ExpandButton::ExpandButton(PressedCallback callback)
    : Button(std::move(callback)) {
  auto* layout_manager = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kNotificationExpandButtonInsets, kNotificationExpandButtonChildSpacing));
  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kEnd);

  TrayPopupUtils::ConfigureTrayPopupButton(this);

  auto label = std::make_unique<views::Label>();
  label->SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                   kNotificationExpandButtonLabelFontSize,
                                   gfx::Font::Weight::MEDIUM));

  label->SetPreferredSize(kNotificationExpandButtonLabelSize);
  label->SetText(base::NumberToString16(total_grouped_notifications_));
  label->SetVisible(ShouldShowLabel());
  label_ = AddChildView(std::move(label));

  UpdateIcons();

  auto image = std::make_unique<views::ImageView>();
  image->SetImage(expanded_ ? expanded_image_ : collapsed_image_);
  image_ = AddChildView(std::move(image));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(), kNotificationExpandButtonCornerRadius);
}

AshNotificationView::ExpandButton::~ExpandButton() = default;

void AshNotificationView::ExpandButton::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;
  expanded_ = expanded;

  label_->SetText(base::NumberToString16(total_grouped_notifications_));
  label_->SetVisible(ShouldShowLabel());

  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);

  SetTooltipText(l10n_util::GetStringUTF16(
      expanded_ ? IDS_ASH_NOTIFICATION_COLLAPSE_TOOLTIP
                : IDS_ASH_NOTIFICATION_EXPAND_TOOLTIP));
  SchedulePaint();
}

bool AshNotificationView::ExpandButton::ShouldShowLabel() const {
  return !expanded_ && total_grouped_notifications_;
}

void AshNotificationView::ExpandButton::UpdateGroupedNotificationsCount(
    int count) {
  total_grouped_notifications_ = count;
  label_->SetText(base::NumberToString16(total_grouped_notifications_));
  label_->SetVisible(ShouldShowLabel());
}

void AshNotificationView::ExpandButton::UpdateIcons() {
  expanded_image_ = gfx::CreateVectorIcon(
      kUnifiedMenuExpandIcon, kNotificationExpandButtonChevronIconSize,
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));

  collapsed_image_ = gfx::ImageSkiaOperations::CreateRotatedImage(
      gfx::CreateVectorIcon(
          kUnifiedMenuExpandIcon, kNotificationExpandButtonChevronIconSize,
          AshColorProvider::Get()->GetContentLayerColor(
              AshColorProvider::ContentLayerType::kIconColorPrimary)),
      SkBitmapOperations::ROTATION_180_CW);
}

gfx::Size AshNotificationView::ExpandButton::CalculatePreferredSize() const {
  if (ShouldShowLabel())
    return kNotificationExpandButtonWithLabelSize;

  return kNotificationExpandButtonSize;
}

void AshNotificationView::ExpandButton::OnThemeChanged() {
  views::Button::OnThemeChanged();

  UpdateIcons();
  image_->SetImage(expanded_ ? expanded_image_ : collapsed_image_);

  views::FocusRing::Get(this)->SetColor(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kFocusRingColor));

  SkColor background_color = AshColorProvider::Get()->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive);
  SetBackground(views::CreateRoundedRectBackground(background_color,
                                                   kTrayItemCornerRadius));
}

AshNotificationView::AshNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup)
    : NotificationViewBase(notification),
      layout_manager_(SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kVertical,
          notification.group_child() ? gfx::Insets() : kNotificationViewPadding,
          0))),
      shown_in_popup_(shown_in_popup) {
  // TODO(crbug/1232197): fix views and layout to match spec.
  // Instantiate view instances and define layout and view hierarchy.
  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kEnd);

  auto control_buttons_view = CreateControlButtonsView();
  control_buttons_view->SetButtonIconColors(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kIconColorPrimary));
  control_buttons_view_ = AddChildView(std::move(control_buttons_view));

  // Header left content contains header row and left content.
  auto header_left_content = std::make_unique<views::View>();
  header_left_content->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, gfx::Insets(), 0));

  auto header_row = CreateHeaderRow();
  header_row->ConfigureLabelsStyle(
      gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL, kHeaderViewLabelSize,
                    gfx::Font::Weight::MEDIUM),
      gfx::Insets(), true);
  header_row->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary));
  header_left_content->AddChildView(std::move(header_row));

  auto left_content = CreateLeftContentView();
  auto* left_content_layout =
      static_cast<views::BoxLayout*>(left_content->GetLayoutManager());
  left_content_layout->set_between_child_spacing(kLeftContentVerticalSpacing);
  left_content_ = header_left_content->AddChildView(std::move(left_content));

  auto content_row = CreateContentRow();
  auto* content_row_layout =
      static_cast<views::BoxLayout*>(content_row->GetLayoutManager());
  content_row_layout->set_inside_border_insets(kContentRowPadding);
  content_row_layout->set_between_child_spacing(kContentRowHorizontalSpacing);

  auto* header_left_content_ptr =
      content_row->AddChildView(std::move(header_left_content));
  content_row_layout->SetFlexForView(header_left_content_ptr, 1);
  content_row->AddChildView(CreateRightContentView());

  expand_button_ = content_row->AddChildView(
      std::make_unique<ExpandButton>(base::BindRepeating(
          &AshNotificationView::ToggleExpand, base::Unretained(this))));

  // TODO(crbug/1241990): add an icon view here.
  auto app_icon_view = std::make_unique<RoundedImageView>(
      kAppIconViewSize / 2, RoundedImageView::Alignment::kCenter);

  // Main right view contains all the views besides control buttons and icon.
  auto main_right_view = std::make_unique<views::View>();
  main_right_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      Orientation::kVertical, kMainRightViewPadding,
      kMainRightViewVerticalSpacing));
  main_right_view->AddChildView(std::move(content_row));
  main_right_view->AddChildView(CreateInlineSettingsView());
  main_right_view->AddChildView(CreateImageContainerView());
  main_right_view->AddChildView(CreateActionsRow());

  // Main view contains all the views besides control buttons.
  auto main_view = std::make_unique<views::View>();
  auto* main_view_layout =
      main_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
          Orientation::kHorizontal, gfx::Insets(), 0));
  main_view_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);
  app_icon_view_ = main_view->AddChildView(std::move(app_icon_view));
  main_view->AddChildView(std::move(main_right_view));
  main_view_ = AddChildView(std::move(main_view));

  collapsed_summary_view_ =
      AddChildView(CreateCollapsedSummaryView(notification));

  auto grouped_notifications_container = std::make_unique<views::View>();
  grouped_notifications_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          Orientation::kVertical, kGroupedNotificationContainerInsets,
          IsExpanded() ? kGroupedNotificationsExpandedSpacing
                       : kGroupedNotificationsCollapsedSpacing));
  grouped_notifications_container_ =
      AddChildView(std::move(grouped_notifications_container));

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

  UpdateWithNotification(notification);
}

AshNotificationView::~AshNotificationView() = default;

void AshNotificationView::ToggleExpand() {
  SetExpanded(!IsExpanded());
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

std::unique_ptr<views::View> AshNotificationView::CreateCollapsedSummaryView(
    const message_center::Notification& notification) {
  auto collapsed_summary_view = std::make_unique<views::View>();
  collapsed_summary_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      kGroupedCollapsedSummaryInsets, kGroupedCollapsedSummaryLabelSpacing));
  collapsed_summary_view->SetVisible(false);

  auto title_label = std::make_unique<views::Label>(
      notification.title(), views::style::CONTEXT_DIALOG_BODY_TEXT);
  title_label->SetMaximumWidthSingleLine(kGroupedCollapsedSummaryTitleLength);

  auto content_label = std::make_unique<views::Label>(
      notification.message(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  content_label->SetMaximumWidthSingleLine(
      kGroupedCollapsedSummaryMessageLength);

  collapsed_summary_view->AddChildView(std::move(title_label));
  collapsed_summary_view->AddChildView(std::move(content_label));

  return collapsed_summary_view;
}

void AshNotificationView::SetGroupedChildExpanded(bool expanded) {
  collapsed_summary_view_->SetVisible(!expanded);
  main_view_->SetVisible(expanded);
  control_buttons_view_->SetVisible(expanded);
  layout_manager_->set_cross_axis_alignment(
      expanded ? views::BoxLayout::CrossAxisAlignment::kEnd
               : views::BoxLayout::CrossAxisAlignment::kStretch);
}

void AshNotificationView::UpdateViewForExpandedState(bool expanded) {
  app_icon_view_->SetBorder(views::CreateEmptyBorder(
      expanded ? kAppIconViewExpandedPadding : kAppIconViewCollapsedPadding));

  bool is_single_expanded_notification =
      !is_grouped_child_view_ && !is_grouped_parent_view_ && expanded;
  header_row()->SetVisible(is_grouped_parent_view_ ||
                           (is_single_expanded_notification));

  // TODO(crbug/1243889): call SizeToFit() `title_view_` to fit the space after
  // the padding and spacing are done.
  if (title_row_) {
    title_row_->UpdateVisibility(is_grouped_child_view_ ||
                                 (IsExpandable() && !expanded));
    title_row_->SetExpanded(expanded);
  }

  if (message_view()) {
    ConfigureLabelStyle(message_view(), kMessageLabelSize, false);

    // TODO(crbug/682266): This is a workaround to that bug by explicitly
    // setting the width. Ideally, we should fix the original bug, but it seems
    // there's no obvious solution for the bug according to
    // https://crbug.com/678337#c7. We will consider making changes to this code
    // when the bug is fixed.
    message_view()->SizeToFit(IsIconViewShown() ? kLeftContentWidth
                                                : kContentRowWidth);
  }

  expand_button_->SetVisible(IsExpandable());
  expand_button_->SetExpanded(expanded);

  static_cast<views::BoxLayout*>(
      grouped_notifications_container_->GetLayoutManager())
      ->set_between_child_spacing(expanded
                                      ? kGroupedNotificationsExpandedSpacing
                                      : kGroupedNotificationsCollapsedSpacing);

  int notification_count = 0;
  for (auto* child : grouped_notifications_container_->children()) {
    auto* notification_view = static_cast<AshNotificationView*>(child);
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

  NotificationViewBase::UpdateWithNotification(notification);
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

  // TODO(crbug/1241990): Since we haven't decided which color we will use for
  // app icon, we will need to change this part later.
  SkColor icon_color = notification.accent_color().value_or(
      AshColorProvider::Get()->GetContentLayerColor(
          ash::AshColorProvider::ContentLayerType::kTextColorPrimary));

  // TODO(crbug.com/768748): figure out if this has a performance impact and
  // cache images if so.
  gfx::Image masked_small_icon = notification.GenerateMaskedSmallIcon(
      kAppIconViewSize, icon_color,
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      AshColorProvider::Get()->GetContentLayerColor(
          ash::AshColorProvider::ContentLayerType::kTextColorPrimary));

  if (masked_small_icon.IsEmpty()) {
    app_icon_view_->SetImage(
        gfx::CreateVectorIcon(message_center::kProductIcon, kAppIconViewSize,
                              SK_ColorWHITE),
        gfx::Size(kAppIconViewSize, kAppIconViewSize));
  } else {
    app_icon_view_->SetImage(masked_small_icon.AsImageSkia(),
                             gfx::Size(kAppIconViewSize, kAppIconViewSize));
  }
}

bool AshNotificationView::IsIconViewShown() const {
  return NotificationViewBase::IsIconViewShown() && !is_grouped_child_view_;
}

void AshNotificationView::SetExpandButtonEnabled(bool enabled) {
  expand_button_->SetVisible(enabled);
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
  return std::make_unique<AshNotificationLabelButton>(std::move(callback),
                                                      label);
}

gfx::Size AshNotificationView::GetIconViewSize() const {
  return gfx::Size(kIconViewSize, kIconViewSize);
}

void AshNotificationView::ToggleInlineSettings(const ui::Event& event) {
  if (!inline_settings_enabled())
    return;
  // TODO(crbug/1233670): Finish the inline settings/blocking UI here.
  NotificationViewBase::ToggleInlineSettings(event);
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

}  // namespace ash
