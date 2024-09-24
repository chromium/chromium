// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/views/ash_notification_view.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/drag_drop/drag_drop_util.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/rounded_image_view.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/pill_button.h"
#include "ash/style/typography.h"
#include "ash/system/notification_center/ash_notification_control_button_factory.h"
#include "ash/system/notification_center/ash_notification_drag_controller.h"
#include "ash/system/notification_center/message_center_constants.h"
#include "ash/system/notification_center/message_center_controller.h"
#include "ash/system/notification_center/message_center_style.h"
#include "ash/system/notification_center/message_center_utils.h"
#include "ash/system/notification_center/message_view_factory.h"
#include "ash/system/notification_center/metrics_utils.h"
#include "ash/system/notification_center/notification_grouping_controller.h"
#include "ash/system/notification_center/notification_style_utils.h"
#include "ash/system/notification_center/views/ash_notification_expand_button.h"
#include "ash/system/notification_center/views/ash_notification_input_container.h"
#include "ash/system/notification_center/views/timestamp_view.h"
#include "ash/wm/work_area_insets.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/notification_view_controller.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/large_image_view.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/notification_view_base.h"
#include "ui/message_center/views/notification_view_util.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/message_center/views/relative_time_formatter.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/drag_utils.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/style/typography.h"
#include "ui/views/view.h"

namespace {

// Used when encoding a notification drop image into binary data. The drop image
// should be resized if its binary size exceeds this limit.
// Use 1 MB as the size limit. On a 256 color image, each pixel takes four bytes
// (RGB + Alpha). The size limit of 1 MB means the maximum pixel count being
// 250K, which should be enough for most notification images without file
// backing.
constexpr size_t kMaxImageSizeInByte = 1000000;

constexpr int kMainRightViewVerticalSpacing = 4;

// This padding is applied to all the children of `main_right_view_` except the
// action buttons.
constexpr auto kMainRightViewChildPadding = gfx::Insets::TLBR(0, 14, 0, 0);

constexpr auto kImageContainerPadding = gfx::Insets::TLBR(12, 14, 12, 12);

constexpr auto kActionButtonsRowPadding = gfx::Insets::TLBR(4, 38, 12, 4);
constexpr int kActionsRowHorizontalSpacing = 8;

constexpr auto kContentRowPadding = gfx::Insets::TLBR(14, 0, 0, 0);

constexpr int kLeftContentVerticalSpacing = 4;
constexpr int kTitleRowMinimumWidthWithIcon = 186;
constexpr int kTitleRowMinimumWidth = 266;
constexpr int kTitleRowSpacing = 6;

// This padding is applied in collapsed mode when there is no message in order
// to vertically center the title.
constexpr auto kTitleRowNoMessageCollapsedPadding =
    gfx::Insets::TLBR(12, 0, 0, 0);

constexpr auto kHeaderRowExpandedPadding = gfx::Insets::TLBR(6, 0, 8, 0);
constexpr auto kHeaderRowCollapsedPadding = gfx::Insets::TLBR(0, 0, 8, 0);
constexpr auto kRightContentCollapsedPadding = gfx::Insets::TLBR(12, 16, 0, 0);
constexpr auto kRightContentExpandedPadding = gfx::Insets::TLBR(20, 16, 0, 0);
constexpr auto kTimeStampInCollapsedStatePadding =
    gfx::Insets::TLBR(0, 0, 0, 16);

constexpr char kGoogleSansFont[] = "Google Sans";

constexpr int kTitleLabelExpandedMaxLines = 2;
constexpr int kTitleLabelCollapsedMaxLines = 1;

// The size for `icon_view_`, which is the icon within right content (between
// title/message view and expand button).
constexpr int kIconViewSize = 48;

// The size for `icon_view_` with RenderArcNotificationsInChrome enabled. UX
// requires reducing the size of the icons but changing it without this feature
// would require updating it for both ash and arc notifications.
constexpr int kIconViewSizeRenderArcInChromeEnabled = 36;

// If the image displayed in `icon_view()` is smaller in either width or height
// than this value, we draw a background around the image.
constexpr int kSmallImageBackgroundThreshold = 6;

// The size of an icon within a control button. Note that this is not the size
// of a control button itself.
constexpr int kControlButtonsIconSize = 14;

int GetTitleCharacterLimit() {
  return message_center::GetNotificationWidth() *
         message_center::kMaxTitleLines /
         message_center::kMinPixelsPerTitleCharacter;
}

// Helpers ---------------------------------------------------------------------

// Create a view that will contain the `content_row`,
// `message_label_in_expanded_state_`, inline settings and the large image.
views::Builder<views::View> CreateMainRightViewBuilder() {
  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets::TLBR(0, 0, kMainRightViewVerticalSpacing, 0))
      .SetOrientation(views::LayoutOrientation::kVertical);

  return views::Builder<views::View>()
      .SetID(message_center::NotificationViewBase::ViewId::kMainRightView)
      .SetLayoutManager(std::move(layout_manager))
      .SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                                   views::MinimumFlexSizeRule::kScaleToZero,
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
      .AddChild(
          views::Builder<views::Label>()
              .SetText(notification.title())
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                         ash::kNotificationTitleLabelSize,
                                         gfx::Font::Weight::MEDIUM)))
      .AddChild(
          views::Builder<views::Label>()
              .SetText(notification.message())
              .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
              .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
              .SetTextStyle(views::style::STYLE_SECONDARY)
              .SetFontList(gfx::FontList({kGoogleSansFont}, gfx::Font::NORMAL,
                                         ash::kNotificationMessageLabelSize,
                                         gfx::Font::Weight::NORMAL)));
}

views::Builder<ash::AshNotificationView::GroupedNotificationsContainer>
CreateGroupedNotificationsContainerBuilder(
    ash::AshNotificationView* parent_notification_view) {
  return views::Builder<
             ash::AshNotificationView::GroupedNotificationsContainer>()
      .SetParentNotificationView(parent_notification_view)
      .SetOrientation(views::BoxLayout::Orientation::kVertical);
}

// Perform a scale and translate animation by scale from (scale_value_x,
// scalue_value_y) and translate from (translate_value_x, translate_value_y) to
// its correct scale and position.
void ScaleAndTranslateView(views::View* view,
                           SkScalar scale_value_x,
                           SkScalar scale_value_y,
                           SkScalar translate_value_x,
                           SkScalar translate_value_y,
                           const std::string& animation_histogram_name) {
  gfx::Transform transform;
  transform.Translate(translate_value_x, translate_value_y);
  transform.Scale(scale_value_x, scale_value_y);

  ui::AnimationThroughputReporter reporter(
      view->layer()->GetAnimator(),
      ash::metrics_util::ForSmoothnessV3(base::BindRepeating(
          [](const std::string& animation_histogram_name, int smoothness) {
            base::UmaHistogramPercentage(animation_histogram_name, smoothness);
          },
          animation_histogram_name)));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(base::TimeDelta())
      .SetTransform(view, transform)
      .Then()
      .SetDuration(
          base::Milliseconds(ash::kLargeImageScaleAndTranslateDurationMs))
      .SetTransform(view, gfx::Transform(), gfx::Tween::ACCEL_0_100_DECEL_80);
}

// Returns the HTML snippet that contains the binary data of `bitmap`. Returns
// `std::nullopt` if having any error.
std::optional<std::u16string> GetHtmlForBitmap(const SkBitmap& bitmap) {
  std::vector<unsigned char> image_data;
  if (gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, /*discard_transparency=*/false,
                                        &image_data)) {
    std::string encoded_data = base::Base64Encode(
        /*input=*/std::string(image_data.cbegin(), image_data.cend()));
    const std::string html = base::StrCat(
        {"<img src=\"data:image/png;base64,", encoded_data, "\"/>"});
    std::u16string html_in_u16;
    if (base::UTF8ToUTF16(html.c_str(), html.size(), &html_in_u16)) {
      return html_in_u16;
    }
  }
  return std::nullopt;
}

}  // namespace

namespace ash {

using CrossAxisAlignment = views::BoxLayout::CrossAxisAlignment;
using MainAxisAlignment = views::BoxLayout::MainAxisAlignment;
using Orientation = views::BoxLayout::Orientation;

BEGIN_METADATA(AshNotificationView, NotificationTitleRow)
END_METADATA

void AshNotificationView::AddedToWidget() {
  MessageView::AddedToWidget();

  // crbug/1337661: We need to abort animations in a grouped parent view when
  // it's widget is being destroyed. By default when a widget is destroyed, all
  // current animations are forced to finish. The grouped notification removal
  // animation triggers an additional resize animation when it is finished. This
  // needs to be aborted explicitly to prevent a crash. We do not need to this
  // observation for grouped notification views.
  if (!is_grouped_child_view_) {
    widget_observation_.Observe(GetWidget());
  }
}

void AshNotificationView::Layout(PassKey) {
  if (is_animating_) {
    return;
  }

  LayoutSuperclass<message_center::NotificationViewBase>(this);
}

void AshNotificationView::GroupedNotificationsContainer::
    ChildPreferredSizeChanged(views::View* view) {
  PreferredSizeChanged();
  parent_notification_view_->GroupedNotificationsPreferredSizeChanged();
}

void AshNotificationView::GroupedNotificationsContainer::
    SetParentNotificationView(AshNotificationView* parent_notification_view) {
  parent_notification_view_ = parent_notification_view;
}

BEGIN_METADATA(AshNotificationView, GroupedNotificationsContainer)
END_METADATA

AshNotificationView::NotificationTitleRow::NotificationTitleRow(
    const std::u16string& title)
    : title_view_(AddChildView(GenerateTitleView(title))),
      title_row_divider_(AddChildView(std::make_unique<views::Label>(
          kNotificationTitleRowDivider,
          views::style::CONTEXT_DIALOG_BODY_TEXT))),
      timestamp_in_collapsed_view_(
          AddChildView(std::make_unique<TimestampView>())) {
  SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(views::LayoutAlignment::kStart,
                  views::LayoutAlignment::kCenter, 1.0,
                  views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kTitleRowSpacing)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter,
                 views::TableLayout::kFixedSize,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddPaddingColumn(views::TableLayout::kFixedSize, kTitleRowSpacing)
      .AddColumn(views::LayoutAlignment::kStart,
                 views::LayoutAlignment::kCenter, 100.0,
                 views::TableLayout::ColumnSize::kUsePreferred, 0, 0)
      .AddRows(1, views::TableLayout::kFixedSize);

  timestamp_in_collapsed_view_->SetProperty(views::kMarginsKey,
                                            kTimeStampInCollapsedStatePadding);
  timestamp_in_collapsed_view_->SetElideBehavior(gfx::ElideBehavior::NO_ELIDE);

  title_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kPreferred));
  title_view_->SetMultiLine(true);
  title_view_->SetAllowCharacterBreak(true);
  title_view_->SetMaxLines(kTitleLabelExpandedMaxLines);

  notification_style_utils::ConfigureLabelStyle(title_row_divider_,
                                                kNotificationSecondaryLabelSize,
                                                /*is_color_primary=*/false);
  message_center_utils::InitLayerForAnimations(title_row_divider_);
  notification_style_utils::ConfigureLabelStyle(timestamp_in_collapsed_view_,
                                                kNotificationSecondaryLabelSize,
                                                /*is_color_primary=*/false);
  message_center_utils::InitLayerForAnimations(timestamp_in_collapsed_view_);
  notification_style_utils::ConfigureLabelStyle(
      title_view_, kNotificationTitleLabelSize,
      /*is_color_primary=*/true, gfx::Font::Weight::MEDIUM);

  ash::TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                             *title_view_);
  title_view_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  timestamp_in_collapsed_view_->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurfaceVariant);
  ash::TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosAnnotation1, *timestamp_in_collapsed_view_);
}

AshNotificationView::NotificationTitleRow::~NotificationTitleRow() = default;

void AshNotificationView::NotificationTitleRow::UpdateTitle(
    const std::u16string& title) {
  title_view_->SetText(title);
}

void AshNotificationView::NotificationTitleRow::UpdateTimestamp(
    base::Time timestamp) {
  timestamp_in_collapsed_view_->SetTimestamp(timestamp);
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
        gfx::Tween::ACCEL_20_DECEL_100,
        "Ash.NotificationView.TitleRowDivider.FadeIn.AnimationSmoothness");
    DCHECK(timestamp_in_collapsed_view_->GetVisible());
    message_center_utils::FadeInView(
        timestamp_in_collapsed_view_, kTitleRowTimestampFadeInAnimationDelayMs,
        kTitleRowTimestampFadeInAnimationDurationMs,
        gfx::Tween::ACCEL_20_DECEL_100,
        "Ash.NotificationView.TimestampInTitle.FadeIn.AnimationSmoothness");
  }
}

void AshNotificationView::NotificationTitleRow::SetMaxAvailableWidth(
    int max_available_width) {
  max_available_width_ = max_available_width;
}

gfx::Size AshNotificationView::NotificationTitleRow::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // TODO(crbug.com/40233803): The size constraint is not passed down from the
  // views tree in the first round of layout, so setting a fixed width to bound
  // the view. The layout manager can size the view beyond this width if there
  // is available space. This works similar to applying a max width on the
  // internal labels.
  return gfx::Size(max_available_width_,
                   GetLayoutManager()->GetPreferredHeightForWidth(
                       this, max_available_width_));
}

void AshNotificationView::NotificationTitleRow::OnThemeChanged() {
  views::View::OnThemeChanged();

  title_view_->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  title_row_divider_->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  timestamp_in_collapsed_view_->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurfaceVariant);
}

AshNotificationView::AshNotificationView(
    const message_center::Notification& notification,
    bool shown_in_popup)
    : NotificationViewBase(notification),
      is_grouped_parent_view_(notification.group_parent()),
      is_grouped_child_view_(notification.group_child()),
      shown_in_popup_(shown_in_popup) {
  if (features::IsNotificationImageDragEnabled()) {
    set_drag_controller(
        Shell::Get()->message_center_controller()->drag_controller());
  }

  message_center_observer_.Observe(message_center::MessageCenter::Get());
  // TODO(crbug.com/40780100): fix views and layout to match spec.
  // Instantiate view instances and define layout and view hierarchy.
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      notification.group_child() ? gfx::Insets() : kNotificationViewPadding));

  auto content_row_layout = std::make_unique<views::FlexLayout>();
  content_row_layout->SetInteriorMargin(kMainRightViewChildPadding);

  auto content_row_builder =
      CreateContentRowBuilder()
          .SetLayoutManager(std::move(content_row_layout))
          .AddChild(
              views::Builder<views::FlexLayoutView>()
                  .SetID(kHeaderLeftContent)
                  .SetOrientation(views::LayoutOrientation::kVertical)
                  .SetInteriorMargin(kContentRowPadding)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kScaleToZero,
                                   views::MaximumFlexSizeRule::kUnbounded))
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
              views::Builder<views::FlexLayoutView>()
                  .SetMainAxisAlignment(views::LayoutAlignment::kEnd)
                  .SetProperty(views::kFlexBehaviorKey,
                               views::FlexSpecification(
                                   views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded))
                  .AddChild(CreateRightContentBuilder().SetProperty(
                      views::kCrossAxisAlignmentKey,
                      views::LayoutAlignment::kStart))
                  .AddChild(
                      views::Builder<views::FlexLayoutView>()
                          .SetOrientation(views::LayoutOrientation::kVertical)
                          .SetCrossAxisAlignment(views::LayoutAlignment::kEnd)
                          .SetMinimumCrossAxisSize(
                              kExpandAndControlButtonsContainerMinimumWidth)
                          .AddChild(
                              views::Builder<views::BoxLayoutView>()
                                  .SetMainAxisAlignment(MainAxisAlignment::kEnd)
                                  .SetMinimumCrossAxisSize(
                                      kControlButtonsContainerMinimumHeight)
                                  .AddChild(
                                      CreateControlButtonsBuilder()
                                          .CopyAddressTo(&control_buttons_view_)
                                          .SetBetweenButtonSpacing(
                                              kNotificationControlButtonsHorizontalSpacing)
                                          .SetCloseButtonIcon(
                                              kNotificationCloseControlButtonIcon)
                                          .SetSettingsButtonIcon(
                                              kNotificationSettingsControlButtonIcon)
                                          .SetButtonIconSize(
                                              kControlButtonsIconSize)
                                          .SetButtonIconColors(
                                              AshColorProvider::Get()
                                                  ->GetContentLayerColor(
                                                      AshColorProvider::
                                                          ContentLayerType::
                                                              kIconColorPrimary))
                                          .SetNotificationControlButtonFactory(
                                              std::make_unique<
                                                  AshNotificationControlButtonFactory>())))
                          .AddChild(
                              views::Builder<AshNotificationExpandButton>()
                                  .CopyAddressTo(&expand_button_)
                                  .SetCallback(base::BindRepeating(
                                      &AshNotificationView::ToggleExpand,
                                      base::Unretained(this))))));
  // TODO(crbug.com/40232718): See View::SetLayoutManagerUseConstrainedSpace.
  content_row()->SetLayoutManagerUseConstrainedSpace(false);

  // Main right view contains all the views besides control buttons, app icon,
  // grouped container and action buttons.
  auto main_right_view_builder =
      CreateMainRightViewBuilder()
          .CopyAddressTo(&main_right_view_)
          .AddChild(content_row_builder)
          .AddChild(
              views::Builder<views::Label>()
                  .CopyAddressTo(&message_label_in_expanded_state_)
                  .SetHorizontalAlignment(gfx::ALIGN_TO_HEAD)
                  .SetMultiLine(true)
                  .SetMaxLines(message_center::kMaxLinesForExpandedMessageLabel)
                  .SetAllowCharacterBreak(true)
                  .SetBorder(
                      views::CreateEmptyBorder(kMainRightViewChildPadding))
                  // TODO(crbug.com/41295639): This is a workaround to that bug
                  // by explicitly setting the width. Ideally, we should fix the
                  // original bug, but it seems there's no obvious solution for
                  // the bug according to https://crbug.com/678337#c7. We will
                  // consider making changes to this code when the bug is fixed.
                  .SetMaximumWidth(GetExpandedMessageLabelWidth()))
          .AddChild(CreateInlineSettingsBuilder())
          .AddChild(CreateSnoozeSettingsBuilder())
          .AddChild(CreateImageContainerBuilder().SetProperty(
              views::kMarginsKey, kImageContainerPadding));

  notification_style_utils::ConfigureLabelStyle(
      message_label_in_expanded_state_, kNotificationMessageLabelSize,
      /*is_color_primary=*/false);

  message_label_in_expanded_state_->SetEnabledColorId(
      cros_tokens::kCrosSysOnSurfaceVariant);
  ash::TypographyProvider::Get()->StyleLabel(
      ash::TypographyToken::kCrosAnnotation1,
      *message_label_in_expanded_state_);

  AddChildView(
      views::Builder<views::FlexLayoutView>()
          .CopyAddressTo(&main_view_)
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .AddChild(views::Builder<views::BoxLayoutView>()
                        .SetID(kAppIconViewContainer)
                        .SetOrientation(Orientation::kVertical)
                        .SetMainAxisAlignment(MainAxisAlignment::kStart)
                        .SetCrossAxisAlignment(CrossAxisAlignment::kStart)
                        .AddChild(views::Builder<RoundedImageView>()
                                      .CopyAddressTo(&app_icon_view_)
                                      .SetCornerRadius(
                                          kNotificationAppIconViewSize / 2)))
          .AddChild(main_right_view_builder)
          .Build());

  AddChildView(CreateCollapsedSummaryBuilder(notification)
                   .CopyAddressTo(&collapsed_summary_view_)
                   .Build());

  // We only need a scroll view if the notification is being shown in its own
  // popup. Scrolling is handled by `UnifiedMessageCenterView` otherwise. Having
  // a nested scroll view results in crbug/1302756.
  if (shown_in_popup) {
    AddChildView(
        views::Builder<views::ScrollView>()
            .CopyAddressTo(&grouped_notifications_scroll_view_)
            .SetBackgroundColor(std::nullopt)
            .SetDrawOverflowIndicator(false)
            .ClipHeightTo(0, std::numeric_limits<int>::max())
            .SetContents(
                CreateGroupedNotificationsContainerBuilder(this).CopyAddressTo(
                    &grouped_notifications_container_))
            .Build());
    static_cast<views::BoxLayout*>(GetLayoutManager())
        ->SetFlexForView(grouped_notifications_scroll_view_, 1);
  } else {
    AddChildView(CreateGroupedNotificationsContainerBuilder(this)
                     .CopyAddressTo(&grouped_notifications_container_)
                     .Build());
  }

  AddChildView(CreateActionsRow(std::make_unique<views::FlexLayout>()));

  // Custom layout and paddings for views in `AshNotificationView`.
  // Note that we also change the padding for some particular views in
  // UpdateViewForExpandedState().
  action_buttons_row()
      ->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetDefault(views::kMarginsKey,
                   gfx::Insets::TLBR(0, 0, 0, kActionsRowHorizontalSpacing))
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
      /*text_view_padding=*/gfx::Insets(), /*auto_color_readability=*/false);

  // This view should not be focusable since it does not act as a button.
  header_row()->SetFocusBehavior(views::View::FocusBehavior::NEVER);

  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);

  // Create layer in some views for animations.
  message_center_utils::InitLayerForAnimations(header_row());
  message_center_utils::InitLayerForAnimations(
      message_label_in_expanded_state_);
  message_center_utils::InitLayerForAnimations(actions_row());

  UpdateWithNotification(notification);
}

AshNotificationView::~AshNotificationView() {
  // b/330585555: We need to abort any in progress animations before we destroy
  // the views hierarchy to make sure there are no dangling pointers associated
  // with an animations' OnAborted callback.
  layer()->GetAnimator()->AbortAllAnimations();
}

void AshNotificationView::SetGroupedChildExpanded(bool expanded) {
  collapsed_summary_view_->SetVisible(!expanded);
  main_view_->SetVisible(expanded);
}

void AshNotificationView::GroupedNotificationsPreferredSizeChanged() {
  PreferredSizeChanged();
}

std::optional<gfx::Rect> AshNotificationView::GetDragAreaBounds() const {
  DCHECK(features::IsNotificationImageDragEnabled());
  if (!IsDraggable()) {
    return std::nullopt;
  }

  const views::View* large_image_view =
      GetViewByID(message_center::NotificationViewBase::kLargeImageView);
  gfx::RectF larget_image_bounds(large_image_view->GetLocalBounds());
  views::View::ConvertRectToTarget(large_image_view, /*target=*/this,
                                   &larget_image_bounds);
  return gfx::ToEnclosedRect(larget_image_bounds);
}

std::optional<gfx::ImageSkia> AshNotificationView::GetDragImage() {
  DCHECK(features::IsNotificationImageDragEnabled());
  if (!IsDraggable()) {
    return std::nullopt;
  }

  // Assume that an Ash notification has at most one large image view. Fetch the
  // image shown in the large image view.
  const gfx::ImageSkia& original_image =
      static_cast<message_center::LargeImageView*>(
          GetViewByID(message_center::NotificationViewBase::kLargeImageView))
          ->drawn_image();

  // Add the background color.
  const std::optional<size_t> radius =
      message_center::notification_view_util::GetLargeImageCornerRadius();
  const gfx::ImageSkia drag_image_with_background =
      gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
          gfx::SizeF{original_image.size()}, radius.value_or(0),
          GetColorProvider()->GetColor(drag_drop::kDragImageBackgroundColor),
          original_image);

  // Add the drop shadow.
  return gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      drag_image_with_background,
      drag_drop::GetDragImageShadowDetails(radius).values);
}

void AshNotificationView::AttachDropData(ui::OSExchangeData* data) {
  DCHECK(IsDraggable());

  // If the notification large image is file-backed, attach the image file path
  // to `data`; otherwise, attach the large image's binary data.
  if (const std::optional<base::FilePath>& image_path =
          message_center::MessageCenter::Get()
              ->FindNotificationById(notification_id())
              ->rich_notification_data()
              .image_path) {
    data->SetFilename(*image_path);
  } else {
    AttachBinaryImageAsDropData(data);
  }
}

bool AshNotificationView::IsDraggable() const {
  // A notification view is draggable only when it contains a large image.
  DCHECK(features::IsNotificationImageDragEnabled());
  return GetViewByID(message_center::NotificationViewBase::kLargeImageView);
}

base::TimeDelta AshNotificationView::GetBoundsAnimationDuration(
    const message_center::Notification& notification) const {
  // This is called after the parent gets notified of
  // `ChildPreferredSizeChanged()`, so the current expanded state is the target
  // state.
  if (!notification.image().IsEmpty()) {
    return base::Milliseconds(kLargeImageExpandAndCollapseAnimationDuration);
  }

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

  if (IsExpanded()) {
    return base::Milliseconds(kGeneralExpandAnimationDuration);
  }
  return base::Milliseconds(kGeneralCollapseAnimationDuration);
}

void AshNotificationView::AnimateGroupedChildExpandedCollapse(bool expanded) {
  message_center_utils::InitLayerForAnimations(collapsed_summary_view_);
  message_center_utils::InitLayerForAnimations(main_view_);
  // Fade out `collapsed_summary_view_`, then fade in `main_view_` in expanded
  // state and vice versa in collapsed state.
  if (expanded) {
    message_center_utils::FadeOutView(
        collapsed_summary_view_,
        OnFadeOutAnimationEndedClosure(collapsed_summary_view_), 0,
        kCollapsedSummaryViewAnimationDurationMs, gfx::Tween::LINEAR,
        "Ash.NotificationView.CollapsedSummaryView.FadeOut."
        "AnimationSmoothness");
    message_center_utils::FadeInView(
        main_view_, kCollapsedSummaryViewAnimationDurationMs,
        kChildMainViewFadeInAnimationDurationMs, gfx::Tween::LINEAR,
        "Ash.NotificationView.MainView.FadeIn.AnimationSmoothness");
    return;
  }

  message_center_utils::FadeOutView(
      main_view_, OnFadeOutAnimationEndedClosure(main_view_), 0,
      kChildMainViewFadeOutAnimationDurationMs, gfx::Tween::LINEAR,
      "Ash.NotificationView.MainView.FadeOut.AnimationSmoothness");
  message_center_utils::FadeInView(
      collapsed_summary_view_, kChildMainViewFadeOutAnimationDurationMs,
      kCollapsedSummaryViewAnimationDurationMs, gfx::Tween::LINEAR,
      "Ash.NotificationView.CollapsedSummaryView.FadeIn.AnimationSmoothness");
}

void AshNotificationView::AnimateSingleToGroup(
    const std::string& notification_id,
    std::string parent_id) {
  ash::message_center_utils::InitLayerForAnimations(left_content());
  ash::message_center_utils::InitLayerForAnimations(right_content());
  ash::message_center_utils::InitLayerForAnimations(
      message_label_in_expanded_state_);
  ash::message_center_utils::InitLayerForAnimations(image_container_view());
  ash::message_center_utils::InitLayerForAnimations(action_buttons_row());

  std::pair<base::OnceClosure, base::OnceClosure> split =
      base::SplitOnceCallback(OnGroupedAnimationEndedClosure(
          left_content_, right_content(), message_label_in_expanded_state_,
          image_container_view(), action_buttons_row(), expand_button_,
          notification_id, parent_id));

  ui::AnimationThroughputReporter reporter(
      left_content()->layer()->GetAnimator(),
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        base::UmaHistogramPercentage(
            "Ash.NotificationView.ConvertSingleToGroup.FadeOut."
            "AnimationSmoothness",
            smoothness);
      })));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(std::move(split.first))
      .OnAborted(std::move(split.second))
      .Once()
      .SetDuration(
          base::Milliseconds(kConvertFromSingleToGroupFadeOutDurationMs))
      .SetOpacity(left_content(), 0.0f, gfx::Tween::LINEAR)
      .SetOpacity(right_content(), 0.0f, gfx::Tween::LINEAR)
      .SetOpacity(message_label_in_expanded_state_, 0.0f, gfx::Tween::LINEAR)
      .SetOpacity(image_container_view(), 0.0f, gfx::Tween::LINEAR)
      .SetOpacity(action_buttons_row(), 0.0f, gfx::Tween::LINEAR);
}

void AshNotificationView::ToggleExpand() {
  const bool target_expanded_state = !IsExpanded();

  SetManuallyExpandedOrCollapsed(
      target_expanded_state ? message_center::ExpandState::USER_EXPANDED
                            : message_center::ExpandState::USER_COLLAPSED);

  if (inline_reply() && inline_reply()->GetVisible()) {
    message_center_utils::FadeOutView(
        inline_reply(), OnFadeOutAnimationEndedClosure(inline_reply()),
        /*delay_in_ms=*/0, kInlineReplyFadeOutAnimationDurationMs,
        gfx::Tween::LINEAR,
        "Ash.NotificationView.InlineReply.FadeOut.AnimationSmoothness");
  }

  SetExpanded(target_expanded_state);

  PerformExpandCollapseAnimation();

  // Log expand button click action.
  if (IsExpanded()) {
    is_grouped_parent_view_
        ? metrics_utils::LogExpandButtonClickAction(
              metrics_utils::ExpandButtonClickAction::EXPAND_GROUP)
        : metrics_utils::LogExpandButtonClickAction(
              metrics_utils::ExpandButtonClickAction::EXPAND_INDIVIDUAL);
  } else {
    is_grouped_parent_view_
        ? metrics_utils::LogExpandButtonClickAction(
              metrics_utils::ExpandButtonClickAction::COLLAPSE_GROUP)
        : metrics_utils::LogExpandButtonClickAction(
              metrics_utils::ExpandButtonClickAction::COLLAPSE_INDIVIDUAL);
  }
}

void AshNotificationView::AddGroupNotification(
    const message_center::Notification& notification) {
  DCHECK(is_grouped_parent_view_);
  // Do not add a grouped notification if a view for it already exists.
  if (FindGroupNotificationView(notification.id())) {
    return;
  }

  auto notification_view =
      std::make_unique<AshNotificationView>(notification,
                                            /*shown_in_popup=*/false);
  notification_view->SetGroupedChildExpanded(IsExpanded());
  notification_view->set_parent_message_view(this);
  notification_view->set_scroller(
      scroller() ? scroller() : grouped_notifications_scroll_view_.get());

  header_row()->SetTimestamp(notification.timestamp());

  grouped_notifications_container_->AddChildViewAt(std::move(notification_view),
                                                   0);

  total_grouped_notifications_++;
  left_content_->SetVisible(false);
  UpdateGroupedNotificationsVisibility();
  expand_button_->UpdateCounter(total_grouped_notifications_);
  PreferredSizeChanged();
}

void AshNotificationView::PopulateGroupNotifications(
    const std::vector<const message_center::Notification*>& notifications) {
  DCHECK(is_grouped_parent_view_);
  // Clear all grouped notifications since we will add all grouped notifications
  // from scratch.
  total_grouped_notifications_ = 0;
  grouped_notifications_container_->RemoveAllChildViews();

  for (auto* notification : notifications) {
    auto notification_view =
        MessageViewFactory::Create(*notification, /*shown_in_popup=*/false);

    auto* child_notification_view =
        static_cast<message_center::MessageView*>(notification_view.get());
    child_notification_view->SetGroupedChildExpanded(IsExpanded());

    if (!total_grouped_notifications_) {
      header_row()->SetTimestamp(notification->timestamp());
    }

    notification_view->SetVisible(
        total_grouped_notifications_ <
            message_center_style::kMaxGroupedNotificationsInCollapsedState ||
        IsExpanded());

    notification_view->set_parent_message_view(this);
    notification_view->set_scroller(
        scroller() ? scroller() : grouped_notifications_scroll_view_.get());

    grouped_notifications_container_->AddChildView(
        std::move(notification_view));

    total_grouped_notifications_++;
  }
  left_content_->SetVisible(total_grouped_notifications_ == 0);
  expand_button_->UpdateCounter(total_grouped_notifications_);
}

void AshNotificationView::RemoveGroupNotification(
    const std::string& notification_id) {
  auto* child_view = FindGroupNotificationView(notification_id);
  if (!child_view) {
    return;
  }

  base::WeakPtr<message_center::MessageView> to_be_removed =
      static_cast<message_center::MessageView*>(child_view)
          ->weak_factory_.GetWeakPtr();
  if (to_be_removed) {
    // Abort any previously queued animations, if a remove animation was in
    // progress this will cause `to_be_removed` to be deleted. Because of this
    // we need to use a weakptr to ensure we do not try to animate an already
    // deleted view.
    to_be_removed->layer()->GetAnimator()->AbortAllAnimations();
  }

  if (!to_be_removed) {
    return;
  }

  auto on_notification_slid_out = base::BindRepeating(
      [](base::WeakPtr<AshNotificationView> self,
         const std::string& notification_id) {
        if (!self) {
          return;
        }

        views::View* to_be_removed =
            self->FindGroupNotificationView(notification_id);
        if (!to_be_removed) {
          return;
        }

        self->total_grouped_notifications_--;
        self->expand_button_->UpdateCounter(self->total_grouped_notifications_);

        self->AnimateResizeAfterRemoval(to_be_removed);
      },
      weak_factory_.GetWeakPtr(), notification_id);

  auto on_animation_aborted = base::BindRepeating(
      [](base::WeakPtr<AshNotificationView> self,
         const std::string& notification_id) {
        if (!self) {
          return;
        }

        views::View* to_be_removed =
            self->FindGroupNotificationView(notification_id);
        if (!to_be_removed) {
          return;
        }

        self->total_grouped_notifications_--;
        self->expand_button_->UpdateCounter(self->total_grouped_notifications_);

        self->grouped_notifications_container_->RemoveChildViewT(to_be_removed);
        self->PreferredSizeChanged();
      },
      weak_factory_.GetWeakPtr(), notification_id);

  // If the removed notification has a layer transform it has already been slid
  // out (For example user swiped it by dragging). We only need to animate a
  // slide out if there is no transform.
  if (to_be_removed && to_be_removed->layer()->transform().IsIdentity()) {
    message_center_utils::SlideOutView(
        to_be_removed.get(), on_notification_slid_out, on_animation_aborted,
        /*delay_in_ms=*/0,
        /*duration_in_ms=*/kSlideOutGroupedNotificationAnimationDurationMs,
        gfx::Tween::LINEAR,
        "Ash.Notification.GroupNotification.SlideOut.AnimationSmoothness");
  } else {
    on_notification_slid_out.Run();
  }
}

void AshNotificationView::UpdateViewForExpandedState(bool expanded) {
  // Grouped parent views should always use the expanded paddings, even if they
  // are collapsed.
  bool use_expanded_padding = expanded || is_grouped_parent_view_;

  header_row()->SetVisible(is_grouped_parent_view_ || expanded);
  header_row()->SetTimestampVisible(!is_grouped_parent_view_ || !expanded);

  if (title_row_) {
    title_row_->UpdateVisibility(IsExpandable() && !expanded);
    title_row_->title_view()->SetMaxLines(
        expanded ? kTitleLabelExpandedMaxLines : kTitleLabelCollapsedMaxLines);
    // Add extra padding to center the title in collapsed mode when there is no
    // message. The exception is when this is a progress notification, as
    // progress notifications always show a progress bar and thus don't need the
    // title vertically centered.
    title_row_->SetProperty(
        views::kMarginsKey,
        !progress_bar_view() && !message_label() && !use_expanded_padding
            ? kTitleRowNoMessageCollapsedPadding
            : gfx::Insets());
  }

  if (message_label()) {
    // `message_label()` is shown only in collapsed mode.
    message_label()->SetVisible(!expanded);
    message_label_in_expanded_state_->SetVisible(expanded &&
                                                 !is_grouped_parent_view_);
  }

  if (progress_bar_view()) {
    int progress_bar_bottom_padding;
    if (!action_buttons().empty()) {
      progress_bar_bottom_padding = kProgressBarWithActionButtonsBottomPadding;
    } else if (use_expanded_padding) {
      progress_bar_bottom_padding = kProgressBarExpandedBottomPadding;
    } else {
      progress_bar_bottom_padding = kProgressBarCollapsedBottomPadding;
    }
    progress_bar_view()->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(message_center::kProgressBarTopPadding, 0,
                          progress_bar_bottom_padding, 0)));
  }

  // Custom padding for app icon and expand button. These 2 views should always
  // use the same padding value so that they are vertical aligned.
  app_icon_view_->SetProperty(views::kMarginsKey,
                              use_expanded_padding ? kAppIconExpandedPadding
                                                   : kAppIconCollapsedPadding);

  if (features::IsRenderArcNotificationsByChromeEnabled()) {
    right_content()->SetProperty(views::kCrossAxisAlignmentKey,
                                 use_expanded_padding
                                     ? views::LayoutAlignment::kStart
                                     : views::LayoutAlignment::kCenter);
  }

  right_content()->SetProperty(
      views::kMarginsKey, use_expanded_padding ? kRightContentExpandedPadding
                                               : kRightContentCollapsedPadding);

  expand_button_->SetProperty(
      views::kMarginsKey, use_expanded_padding ? kExpandButtonExpandedPadding
                                               : kExpandButtonCollapsedPadding);
  header_row()->SetProperty(views::kMarginsKey,
                            use_expanded_padding ? kHeaderRowExpandedPadding
                                                 : kHeaderRowCollapsedPadding);

  expand_button_->SetExpanded(expanded);

  if (is_grouped_parent_view_) {
    if (grouped_notifications_scroll_view_) {
      grouped_notifications_scroll_view_->ClipHeightTo(
          0, CalculateMaxHeightForGroupedNotifications());
    }

    auto* grouped_notifications_container_layout_manager =
        static_cast<views::BoxLayout*>(
            grouped_notifications_container_->GetLayoutManager());
    grouped_notifications_container_layout_manager->set_inside_border_insets(
        expanded ? kGroupedNotificationContainerExpandedInsets
                 : kGroupedNotificationContainerCollapsedInsets);
    grouped_notifications_container_layout_manager->set_between_child_spacing(
        expanded ? kGroupedNotificationsExpandedSpacing
                 : kGroupedNotificationsCollapsedSpacing);

    int notification_count = 0;
    for (views::View* child : grouped_notifications_container_->children()) {
      auto* notification_view =
          static_cast<message_center::MessageView*>(child);
      notification_view->AnimateGroupedChildExpandedCollapse(expanded);
      notification_view->SetGroupedChildExpanded(expanded);

      notification_count++;
      if (notification_count >
          message_center_style::kMaxGroupedNotificationsInCollapsedState) {
        child->SetVisible(expanded);
      }
    }
  }

  NotificationViewBase::UpdateViewForExpandedState(expanded);

  message_label_in_expanded_state_->SetProperty(
      views::kMarginsKey,
      (actions_row()->GetVisible() || image_container_view()->GetVisible() ||
               is_grouped_child_view_
           ? kMessageLabelInExpandedStatePadding
           : kMessageLabelInExpandedStateExtendedPadding));
}

void AshNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  is_grouped_child_view_ = notification.group_child();
  is_grouped_parent_view_ = notification.group_parent();

  if (grouped_notifications_scroll_view_) {
    grouped_notifications_scroll_view_->SetVisible(is_grouped_parent_view_);
  }
  grouped_notifications_container_->SetVisible(is_grouped_parent_view_);

  if (is_grouped_child_view_ && !is_nested()) {
    SetIsNested();
  }

  header_row()->SetIsInGroupChildNotification(is_grouped_child_view_);
  UpdateMessageLabelInExpandedState(notification);

  NotificationViewBase::UpdateWithNotification(notification);

  CreateOrUpdateSnoozeButton(notification);

  // Configure views style.
  UpdateIconAndButtonsColor(&notification);
  if (message_label()) {
    notification_style_utils::ConfigureLabelStyle(message_label(),
                                                  kNotificationMessageLabelSize,
                                                  /*is_color_primary=*/false);
    message_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
    ash::TypographyProvider::Get()->StyleLabel(
        ash::TypographyToken::kCrosAnnotation1, *message_label());
  }
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
      left_content()->RemoveChildViewT(title_row_.get());
      title_row_ = nullptr;
    }
    return;
  }

  const std::u16string& title = gfx::TruncateString(
      notification.title(), GetTitleCharacterLimit(), gfx::WORD_BREAK);

  if (!title_row_) {
    title_row_ =
        AddViewToLeftContent(std::make_unique<NotificationTitleRow>(title));
  } else {
    title_row_->UpdateTitle(title);
    ReorderViewInLeftContent(title_row_);
  }

  expand_button_->SetNotificationTitleForButtonTooltip(title);

  int max_available_width = notification.icon().IsEmpty()
                                ? kTitleRowMinimumWidth
                                : kTitleRowMinimumWidthWithIcon;
  if (shown_in_popup_) {
    max_available_width -= message_center::GetNotificationWidth() -
                           GetNotificationInMessageCenterWidth();
  }
  title_row_->SetMaxAvailableWidth(max_available_width);

  title_row_->UpdateTimestamp(notification.timestamp());
}

void AshNotificationView::CreateOrUpdateSmallIconView(
    const message_center::Notification& notification) {
  if (is_grouped_child_view_ && !notification.icon().IsEmpty()) {
    app_icon_view_->SetImage(
        notification.icon().Rasterize(GetColorProvider()),
        gfx::Size(kNotificationAppIconViewSize, kNotificationAppIconViewSize));
    return;
  }

  UpdateAppIconView(&notification);
}

void AshNotificationView::CreateOrUpdateInlineSettingsViews(
    const message_center::Notification& notification) {
  if (inline_settings_enabled()) {
    DCHECK(message_center::SettingsButtonHandler::INLINE ==
           notification.rich_notification_data().settings_button_handler);
    return;
  }

  set_inline_settings_enabled(
      !is_grouped_child_view_ &&
      notification.rich_notification_data().settings_button_handler ==
          message_center::SettingsButtonHandler::INLINE);

  if (!inline_settings_enabled()) {
    return;
  }

  inline_settings_row()->AddChildView(
      notification_style_utils::CreateInlineSettingsViewForMessageView(this));
}

void AshNotificationView::CreateOrUpdateSnoozeSettingsViews(
    const message_center::Notification& notification) {
  // TODO(b/298216201): Enable snooze settings after adding mojo callbacks in
  // the snooze settings layout.

  if (!snooze_settings_enabled()) {
    return;
  }

  auto snooze_notification_1_hour_button = GenerateNotificationLabelButton(
      base::BindRepeating(&MessageView::DisableNotification,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_SNOOZE_SETTINGS_SNOOZE_1_HOUR_TEXT));
  snooze_settings_row()->AddChildView(
      std::move(snooze_notification_1_hour_button));

  auto snooze_notification_15_minutes_button = GenerateNotificationLabelButton(
      base::BindRepeating(&MessageView::DisableNotification,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_SNOOZE_SETTINGS_SNOOZE_15_MINUTES_TEXT));
  snooze_settings_row()->AddChildView(
      std::move(snooze_notification_15_minutes_button));

  auto snooze_notification_30_minutes_button = GenerateNotificationLabelButton(
      base::BindRepeating(&MessageView::DisableNotification,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_SNOOZE_SETTINGS_SNOOZE_30_MINUTES_TEXT));
  snooze_settings_row()->AddChildView(
      std::move(snooze_notification_30_minutes_button));

  auto snooze_notification_2_hours_button = GenerateNotificationLabelButton(
      base::BindRepeating(&MessageView::DisableNotification,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_SNOOZE_SETTINGS_SNOOZE_2_HOURS_TEXT));
  snooze_settings_row()->AddChildView(
      std::move(snooze_notification_2_hours_button));

  auto undo_snooze_notification_button = GenerateNotificationLabelButton(
      base::BindRepeating(&AshNotificationView::ToggleSnoozeSettings,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_NOTIFICATION_SNOOZE_SETTINGS_UNDO_SNOOZE_TEXT));
  snooze_settings_row()->AddChildView(
      std::move(undo_snooze_notification_button));
}

void AshNotificationView::CreateOrUpdateCompactTitleMessageView(
    const message_center::Notification& notification) {
  // No CompactTitleMessageView required. It is only used for progress
  // notifications when the notification is collapsed, and Ash progress
  // notifications only show a progress bar when collapsed.
}

void AshNotificationView::CreateOrUpdateProgressViews(
    const message_center::Notification& notification) {
  // AshNotificationView should have the status view, followed by the progress
  // bar. This is the opposite of what is required of the chrome notification.
  CreateOrUpdateProgressStatusView(notification);
  CreateOrUpdateProgressBarView(notification);
  if (progress_bar_view()) {
    progress_bar_view()->SetForegroundColorId(cros_tokens::kCrosSysPrimary);
    progress_bar_view()->SetBackgroundColorId(
        cros_tokens::kCrosSysHighlightShape);
  }

  if (status_view()) {
    status_view()->SetMultiLine(true);
    status_view()->SetMaxLines(message_center::kMaxLinesForStatusView);
    status_view()->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosAnnotation1,
                                          *status_view());
  }
}

void AshNotificationView::UpdateControlButtonsVisibility() {
  NotificationViewBase::UpdateControlButtonsVisibility();

  // Always hide snooze button in control buttons since we show this snooze
  // button in actions button view.
  control_buttons_view()->ShowSnoozeButton(false);

  // Hide settings button for grouped child notifications.
  if (is_grouped_child_view_) {
    control_buttons_view()->ShowSettingsButton(false);
  }
}

bool AshNotificationView::IsIconViewShown() const {
  return NotificationViewBase::IsIconViewShown() && !is_grouped_child_view_;
}

void AshNotificationView::SetExpandButtonVisibility(bool visible) {
  expand_button_->SetVisible(visible);
}

bool AshNotificationView::IsExpandable() const {
  // Inline settings can not be expanded.
  if (GetMode() == Mode::SETTING) {
    return false;
  }

  // Notification should always be expandable since we hide `header_row()` in
  // collapsed state.
  return true;
}

void AshNotificationView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  // Call parent's SetCornerRadius to update radius used for highlight path.
  NotificationViewBase::SetCornerRadius(top_radius, bottom_radius);
}

void AshNotificationView::OnThemeChanged() {
  views::View::OnThemeChanged();

  if (message_label()) {
    message_label()->SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant);
  }

  if (control_buttons_view_) {
    control_buttons_view_->SetButtonIconColors(
        AshColorProvider::Get()->GetContentLayerColor(
            AshColorProvider::ContentLayerType::kIconColorPrimary));
  }

  if (message_label_in_expanded_state_) {
    message_label_in_expanded_state_->SetEnabledColorId(
        cros_tokens::kCrosSysOnSurfaceVariant);
  }

  UpdateIconAndButtonsColor(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id()));

  // For unittests, `GetColorProvider()` could be nullptr.
  if (inline_reply() && GetColorProvider()) {
    inline_reply()->textfield()->SetTextColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurface));
    inline_reply()->textfield()->set_placeholder_text_color(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnSurfaceVariant));
  }

  if (icon_view() &&
      (right_content()->width() - icon_view()->GetImageDrawingSize().width() >
           kSmallImageBackgroundThreshold ||
       right_content()->height() - icon_view()->GetImageDrawingSize().height() >
           kSmallImageBackgroundThreshold)) {
    icon_view()->set_apply_rounded_corners(false);
    right_content()->SetBackground(views::CreateThemedRoundedRectBackground(
        kColorAshControlBackgroundColorInactive,
        message_center::kImageCornerRadius));
  }
}

std::unique_ptr<message_center::NotificationInputContainer>
AshNotificationView::GenerateNotificationInputContainer() {
  return std::make_unique<AshNotificationInputContainer>(this);
}

std::unique_ptr<views::LabelButton>
AshNotificationView::GenerateNotificationLabelButton(
    views::Button::PressedCallback callback,
    const std::u16string& label) {
  std::unique_ptr<PillButton> actions_button = std::make_unique<PillButton>(
      std::move(callback), label, PillButton::Type::kFloatingWithoutIcon,
      /*icon=*/nullptr, kNotificationPillButtonHorizontalSpacing);
  actions_button->SetButtonTextColorId(cros_tokens::kCrosSysOnSurface);

  return actions_button;
}

gfx::Size AshNotificationView::GetIconViewSize() const {
  int icon_size = features::IsRenderArcNotificationsByChromeEnabled()
                      ? kIconViewSizeRenderArcInChromeEnabled
                      : kIconViewSize;
  return gfx::Size(icon_size, icon_size);
}

int AshNotificationView::GetLargeImageViewMaxWidth() const {
  return message_center::GetNotificationWidth() -
         kNotificationViewPadding.width() - kNotificationAppIconViewSize -
         kMainRightViewChildPadding.width();
}

void AshNotificationView::ToggleInlineSettings(const ui::Event& event) {
  if (!inline_settings_enabled()) {
    return;
  }

  bool should_show_inline_settings = !inline_settings_row()->GetVisible();
  PerformToggleInlineSettingsAnimation(should_show_inline_settings);

  NotificationViewBase::ToggleInlineSettings(event);

  if (is_grouped_parent_view_) {
    if (shown_in_popup_) {
      grouped_notifications_scroll_view_->SetVisible(
          !should_show_inline_settings);
    } else {
      grouped_notifications_container_->SetVisible(
          !should_show_inline_settings);
    }
  } else {
    // In settings UI, we only show the app icon and header row along with the
    // inline settings UI.
    header_row()->SetVisible(true);
    left_content()->SetVisible(!should_show_inline_settings);
    right_content()->SetVisible(!should_show_inline_settings);
  }
  expand_button_->SetVisible(!should_show_inline_settings);

  PreferredSizeChanged();
}

void AshNotificationView::ToggleSnoozeSettings(const ui::Event& event) {
  if (!snooze_settings_enabled()) {
    return;
  }

  bool should_show_snooze_settings = !snooze_settings_row()->GetVisible();

  NotificationViewBase::ToggleSnoozeSettings(event);

  left_content()->SetVisible(!should_show_snooze_settings);
  right_content()->SetVisible(!should_show_snooze_settings);

  PreferredSizeChanged();
}

void AshNotificationView::OnInlineReplyUpdated() {
  DCHECK(inline_reply() && inline_reply()->GetVisible());
  // Fade out actions button and then fade in inline reply.
  message_center_utils::InitLayerForAnimations(action_buttons_row());
  message_center_utils::FadeOutView(
      action_buttons_row(),
      OnFadeOutAnimationEndedClosure(action_buttons_row()),
      /*delay_in_ms=*/0, kActionButtonsFadeOutAnimationDurationMs,
      gfx::Tween::LINEAR,
      "Ash.NotificationView.ActionButtonsRow.FadeOut.AnimationSmoothness");

  // Delay for the action buttons to fade out, then fade in inline reply.
  message_center_utils::InitLayerForAnimations(inline_reply());
  message_center_utils::FadeInView(
      inline_reply(), kActionButtonsFadeOutAnimationDurationMs,
      kInlineReplyFadeInAnimationDurationMs, gfx::Tween::LINEAR,
      "Ash.NotificationView.InlineReply.FadeIn.AnimationSmoothness");
}

views::View* AshNotificationView::FindGroupNotificationView(
    const std::string& notification_id) {
  auto notification = base::ranges::find(
      grouped_notifications_container_->children(), notification_id,
      [](views::View* notification_view) {
        return static_cast<message_center::MessageView*>(notification_view)
            ->notification_id();
      });
  return notification == grouped_notifications_container_->children().end()
             ? nullptr
             : *notification;
}

std::vector<raw_ptr<views::LabelButton, VectorExperimental>>
AshNotificationView::GetActionButtonsForTest() {
  return action_buttons();
}

views::Label* AshNotificationView::GetTitleRowLabelForTest() {
  return title_row_->title_view();
}

message_center::NotificationInputContainer*
AshNotificationView::GetInlineReplyForTest() {
  return inline_reply();
}

void AshNotificationView::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (!is_grouped_parent_view_) {
    return;
  }

  RemoveGroupNotification(notification_id);
}

void AshNotificationView::OnWidgetClosing(views::Widget* widget) {
  widget_observation_.Reset();
  AbortAllAnimations();
}

void AshNotificationView::OnWidgetDestroying(views::Widget* widget) {
  OnWidgetClosing(widget);
}

void AshNotificationView::AbortAllAnimations() {
  std::vector<scoped_refptr<ui::LayerAnimator>> animators;
  animators.push_back(layer()->GetAnimator());
  for (views::View* child_notification :
       grouped_notifications_container_->children()) {
    animators.push_back(child_notification->layer()->GetAnimator());
  }

  for (auto animator : animators) {
    animator->AbortAllAnimations();
  }
}

void AshNotificationView::CreateOrUpdateSnoozeButton(
    const message_center::Notification& notification) {
  if (!notification.should_show_snooze_button()) {
    if (action_buttons_row()->Contains(snooze_button_)) {
      action_buttons_row()->RemoveChildViewT(snooze_button_.get());
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
    action_buttons_row()->ReorderChildView(
        snooze_button_spacer_, action_buttons_row()->children().size());
    action_buttons_row()->ReorderChildView(
        snooze_button_, action_buttons_row()->children().size());
    return;
  }

  action_buttons_row()->AddChildView(
      views::Builder<views::BoxLayoutView>()
          .CopyAddressTo(&snooze_button_spacer_)
          .SetMainAxisAlignment(MainAxisAlignment::kEnd)
          .SetProperty(views::kBoxLayoutFlexKey,
                       views::BoxLayoutFlexSpecification())
          .Build());

  auto snooze_button = std::make_unique<IconButton>(
      base::BindRepeating(&AshNotificationView::OnSnoozeButtonPressed,
                          base::Unretained(this)),
      IconButton::Type::kMediumFloating, &kNotificationSnoozeButtonIcon,
      IDS_MESSAGE_CENTER_NOTIFICATION_SNOOZE_BUTTON_TOOLTIP);
  snooze_button_ = action_buttons_row()->AddChildView(std::move(snooze_button));
}

void AshNotificationView::UpdateGroupedNotificationsVisibility() {
  for (size_t i = 0; i < grouped_notifications_container_->children().size();
       i++) {
    auto* view = grouped_notifications_container_->children()[i].get();
    bool show_notification_view =
        IsExpanded() ||
        i < message_center_style::kMaxGroupedNotificationsInCollapsedState;

    if (view->GetVisible() == show_notification_view) {
      continue;
    }

    view->SetVisible(show_notification_view);
  }
}

void AshNotificationView::UpdateMessageLabelInExpandedState(
    const message_center::Notification& notification) {
  if (notification.type() == message_center::NOTIFICATION_TYPE_PROGRESS ||
      notification.message().empty()) {
    message_label_in_expanded_state_->SetVisible(false);
    return;
  }
  message_label_in_expanded_state_->SetText(gfx::TruncateString(
      notification.message(), message_center::GetMessageCharacterLimit(),
      gfx::WORD_BREAK));

  message_label_in_expanded_state_->SetVisible(true);
}

int AshNotificationView::GetExpandedMessageLabelWidth() {
  int notification_width = shown_in_popup_
                               ? message_center::GetNotificationWidth()
                               : GetNotificationInMessageCenterWidth();

  return notification_width - kNotificationViewPadding.width() -
         kNotificationAppIconViewSize - kMainRightViewChildPadding.width() -
         kMessageLabelInExpandedStatePadding.width();
}

void AshNotificationView::UpdateAppIconView(
    const message_center::Notification* notification) {
  // Grouped child notification use notification's icon for the app icon view,
  // so we don't need further update here.
  if (!notification ||
      (is_grouped_child_view_ && !notification->icon().IsEmpty())) {
    return;
  }

  app_icon_view_->SetImage(
      notification_style_utils::CreateNotificationAppIcon(notification));
}

void AshNotificationView::UpdateIconAndButtonsColor(
    const message_center::Notification* notification) {
  UpdateAppIconView(notification);

  SkColor button_color =
      notification_style_utils::CalculateIconBackgroundColor(notification);
  bool use_default_button_color =
      !notification ||
      notification->rich_notification_data().ignore_accent_color_for_text;
  if (use_default_button_color) {
    button_color = AshColorProvider::Get()->GetControlsLayerColor(
        AshColorProvider::ControlsLayerType::kControlBackgroundColorActive);
  }

  for (views::LabelButton* action_button : action_buttons()) {
    static_cast<PillButton*>(action_button)->SetButtonTextColor(button_color);
  }

  if (snooze_button_) {
    snooze_button_->SetIconColor(button_color);
  }
}

void AshNotificationView::AnimateResizeAfterRemoval(
    views::View* to_be_removed) {
  auto on_resize_complete = base::BindRepeating(
      [](base::WeakPtr<AshNotificationView> self) {
        if (!self) {
          return;
        }

        self->set_is_animating(false);

        if (self->shown_in_popup_) {
          self->grouped_notifications_scroll_view_
              ->DeprecatedLayoutImmediately();
        }
      },
      weak_factory_.GetWeakPtr());

  int group_container_previous_height =
      grouped_notifications_container_->height();
  size_t removed_index =
      grouped_notifications_container_->GetIndexOf(to_be_removed).value();

  grouped_notifications_container_->RemoveChildViewT(to_be_removed).reset();

  auto* notification_view_controller = message_center_utils::
      GetActiveNotificationViewControllerForNotificationView(this);
  if (notification_view_controller) {
    notification_view_controller->AnimateResize();
  }

  if (shown_in_popup_) {
    grouped_notifications_scroll_view_->DeprecatedLayoutImmediately();
  } else {
    DeprecatedLayoutImmediately();
    PreferredSizeChanged();
  }

  int grouped_container_height_reduction =
      group_container_previous_height -
      grouped_notifications_container_->height();

  views::AnimationBuilder animation_builder;

  if (grouped_notifications_container_->children().begin() + removed_index >=
      grouped_notifications_container_->children().end()) {
    return;
  }
  set_is_animating(true);
  animation_builder.OnEnded(on_resize_complete);
  for (auto it =
           grouped_notifications_container_->children().begin() + removed_index;
       it != grouped_notifications_container_->children().end(); it++) {
    gfx::Rect child_bounds = (*it)->layer()->GetTargetBounds();
    (*it)->layer()->SetBounds(gfx::Rect(
        child_bounds.x(), child_bounds.y() + grouped_container_height_reduction,
        child_bounds.width(), child_bounds.height()));

    animation_builder.Once()
        .SetDuration(base::Milliseconds(
            message_center::kNotificationResizeAnimationDurationMs))
        .SetBounds((*it), child_bounds, gfx::Tween::EASE_OUT);
  }
}

void AshNotificationView::PerformExpandCollapseAnimation() {
  if (title_row_) {
    title_row_->PerformExpandCollapseAnimation();
  }

  // Fade in `header row()` if this is not a grouped parent view.
  if (header_row() && header_row()->GetVisible() && !is_grouped_parent_view_) {
    message_center_utils::FadeInView(
        header_row(), kHeaderRowFadeInAnimationDelayMs,
        kHeaderRowFadeInAnimationDurationMs, gfx::Tween::LINEAR,
        "Ash.NotificationView.HeaderRow.FadeIn.AnimationSmoothness");
  }

  // Fade in `message_label()`. We only do fade in for both message view in
  // expanded and collapsed mode if there's a difference between them (a.k.a
  // when `message_label()` is truncated).
  if (message_label() && message_label()->GetVisible() &&
      IsMessageLabelTruncated()) {
    message_center_utils::InitLayerForAnimations(message_label());
    message_center_utils::FadeInView(
        message_label(), kMessageLabelFadeInAnimationDelayMs,
        kMessageLabelFadeInAnimationDurationMs, gfx::Tween::LINEAR,
        "Ash.NotificationView.MessageLabel.FadeIn.AnimationSmoothness");
  }

  // Fade in `message_label_in_expanded_state_`.
  if (message_label_in_expanded_state_ &&
      message_label_in_expanded_state_->GetVisible() && message_label() &&
      IsMessageLabelTruncated()) {
    message_center_utils::FadeInView(
        message_label_in_expanded_state_,
        kMessageLabelInExpandedStateFadeInAnimationDelayMs,
        kMessageLabelInExpandedStateFadeInAnimationDurationMs,
        gfx::Tween::LINEAR,
        "Ash.NotificationView.ExpandedMessageLabel.FadeIn.AnimationSmoothness");
  }

  if (!image_container_view()->children().empty() && icon_view()) {
    PerformLargeImageAnimation();
  }

  if (actions_row() && actions_row()->GetVisible()) {
    message_center_utils::FadeInView(
        actions_row(), kActionsRowFadeInAnimationDelayMs,
        kActionsRowFadeInAnimationDurationMs, gfx::Tween::LINEAR,
        "Ash.NotificationView.ActionsRow.FadeIn.AnimationSmoothness");
  }

  if (total_grouped_notifications_) {
    // Ensure layout is up-to-date before animating expand button. This is used
    // for its bounds animation.
    if (needs_layout()) {
      DeprecatedLayoutImmediately();
    }
    auto* notification =
        message_center::MessageCenter::Get()->FindNotificationById(
            notification_id());

    // When the notification is ArcNotification and not group parent, the view
    // is rendered in Android then attached to message center. Ash does not
    // directly control the layout so we should not check `needs_layout()`.
    if (message_center_utils::IsAshNotification(notification) &&
        !notification->group_parent()) {
      DCHECK(!needs_layout());
    }

    expand_button_->AnimateExpandCollapse();
  }
}

void AshNotificationView::PerformLargeImageAnimation() {
  message_center_utils::InitLayerForAnimations(image_container_view());
  message_center_utils::InitLayerForAnimations(icon_view());
  auto icon_view_bounds = icon_view()->GetBoundsInScreen();
  auto large_image_bounds = image_container_view()->GetBoundsInScreen();

  if (IsExpanded()) {
    // In expanded state, do a scale and translate from `icon_view()` to
    // `image_container_view()`.
    message_center_utils::InitLayerForAnimations(image_container_view());
    ScaleAndTranslateView(image_container_view(),
                          static_cast<double>(icon_view()->width()) /
                              image_container_view()->width(),
                          static_cast<double>(icon_view()->height()) /
                              image_container_view()->height(),
                          icon_view_bounds.x() - large_image_bounds.x(),
                          icon_view_bounds.y() - large_image_bounds.y(),
                          "Ash.NotificationView.ImageContainerView."
                          "ScaleAndTranslate.AnimationSmoothness");

    // If we use different images for `icon_view()` and `image_container_view()`
    // (a.k.a hide_icon_on_expanded() is false), fade in
    // `image_container_view()`.
    if (!hide_icon_on_expanded()) {
      message_center_utils::FadeInView(
          image_container_view(), kLargeImageFadeInAnimationDelayMs,
          kLargeImageFadeInAnimationDurationMs, gfx::Tween::LINEAR,
          "Ash.NotificationView.ImageContainerView.FadeIn.AnimationSmoothness");
    }
    return;
  }

  if (hide_icon_on_expanded()) {
    // In collapsed state, if we use a same image for `icon_view()` and
    // `image_container_view()`, perform a scale and translate from
    // `image_container_view()` to `icon_view()`.
    ScaleAndTranslateView(
        icon_view(),
        static_cast<double>(image_container_view()->width()) /
            icon_view()->width(),
        static_cast<double>(image_container_view()->height()) /
            icon_view()->height(),
        large_image_bounds.x() - icon_view_bounds.x(),
        large_image_bounds.y() - icon_view_bounds.y(),
        "Ash.NotificationView.IconView.ScaleAndTranslate.AnimationSmoothness");
    return;
  }

  // In collapsed state, if we use a different image for `icon_view()` and
  // `image_container_view()`, fade out and scale down `image_container_view()`.
  message_center_utils::FadeOutView(
      image_container_view(),
      OnFadeOutAnimationEndedClosure(image_container_view()),
      kLargeImageFadeOutAnimationDelayMs, kLargeImageFadeOutAnimationDurationMs,
      gfx::Tween::ACCEL_20_DECEL_100,
      "Ash.NotificationView.ImageContainerView.FadeOut.AnimationSmoothness");

  gfx::Transform transform;
  // Translate y further down so that it would not interfere with the currently
  // shown `icon_view()`.
  transform.Translate((icon_view_bounds.x() - large_image_bounds.x()),
                      (icon_view_bounds.y() - large_image_bounds.y() +
                       large_image_bounds.height()));
  transform.Scale(static_cast<double>(icon_view()->width()) /
                      image_container_view()->width(),
                  static_cast<double>(icon_view()->height()) /
                      image_container_view()->height());

  ui::AnimationThroughputReporter reporter(
      image_container_view()->layer()->GetAnimator(),
      ash::metrics_util::ForSmoothnessV3(
          base::BindRepeating([](int smoothness) {
            base::UmaHistogramPercentage(
                "Ash.NotificationView.ImageContainerView.ScaleDown."
                "AnimationSmoothness",
                smoothness);
          })));

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .At(base::TimeDelta())
      .SetTransform(image_container_view(), gfx::Transform())
      .Then()
      .SetDuration(base::Milliseconds(kLargeImageScaleDownDurationMs))
      .SetTransform(image_container_view(), transform,
                    gfx::Tween::ACCEL_20_DECEL_100);
}

void AshNotificationView::PerformToggleInlineSettingsAnimation(
    bool should_show_inline_settings) {
  if (ui::ScopedAnimationDurationScaleMode::duration_multiplier() ==
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    return;
  }

  message_center_utils::InitLayerForAnimations(main_right_view_);
  message_center_utils::InitLayerForAnimations(inline_settings_row());

  // Fade out views.
  if (should_show_inline_settings) {
    // Fade out left_content if it's visible.
    if (left_content_->GetVisible()) {
      message_center_utils::InitLayerForAnimations(left_content());
      message_center_utils::FadeOutView(
          left_content(), OnFadeOutAnimationEndedClosure(left_content()),
          /*delay_in_ms=*/0, kToggleInlineSettingsFadeOutDurationMs,
          gfx::Tween::LINEAR,
          "Ash.NotificationView.LeftContent.FadeOut.AnimationSmoothness");
    }
    message_center_utils::FadeOutView(
        expand_button_, OnFadeOutAnimationEndedClosure(expand_button_),
        /*delay_in_ms=*/0, kToggleInlineSettingsFadeOutDurationMs,
        gfx::Tween::LINEAR,
        "Ash.NotificationView.ExpandButton.FadeOut.AnimationSmoothness");

    // Fade out icon_view() if it exists.
    if (icon_view()) {
      message_center_utils::InitLayerForAnimations(icon_view());
      message_center_utils::FadeOutView(
          icon_view(), OnFadeOutAnimationEndedClosure(icon_view()),
          /*delay_in_ms=*/0, kToggleInlineSettingsFadeOutDurationMs,
          gfx::Tween::LINEAR,
          "Ash.NotificationView.IconView.FadeOut.AnimationSmoothness");
    }
  } else {
    message_center_utils::FadeOutView(
        inline_settings_row(),
        OnFadeOutAnimationEndedClosure(inline_settings_row()),
        /*delay_in_ms=*/0, kToggleInlineSettingsFadeOutDurationMs,
        gfx::Tween::LINEAR,
        "Ash.NotificationView.InlineSettingsRow.FadeOut.AnimationSmoothness");
  }

  // Fade in views.
  message_center_utils::FadeInView(
      main_right_view_, kToggleInlineSettingsFadeInDelayMs,
      kToggleInlineSettingsFadeInDurationMs, gfx::Tween::LINEAR,
      "Ash.NotificationView.MainRightView.FadeIn.AnimationSmoothness");
}

void AshNotificationView::AnimateSingleToGroupFadeIn() {
  auto fade_in_view = shown_in_popup_ ? grouped_notifications_scroll_view_
                                      : grouped_notifications_container_;
  message_center_utils::InitLayerForAnimations(fade_in_view);
  message_center_utils::FadeInView(
      fade_in_view, /*delay_in_ms=*/0,
      kConvertFromSingleToGroupFadeInDurationMs, gfx::Tween::LINEAR,
      "Ash.NotificationView.ConvertSingleToGroup.FadeIn.AnimationSmoothness");
}

int AshNotificationView::CalculateMaxHeightForGroupedNotifications() {
  auto* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  const WorkAreaInsets* work_area =
      WorkAreaInsets::ForWindow(shelf->GetWindow()->GetRootWindow());

  const int bottom = shelf->IsHorizontalAlignment()
                         ? shelf->GetShelfBoundsInScreen().y()
                         : work_area->user_work_area_bounds().bottom();

  const int free_space_height_above_anchor =
      bottom - work_area->user_work_area_bounds().y();

  const int vertical_margin = 2 * message_center::kMarginBetweenPopups +
                              kNotificationViewPadding.height();

  return free_space_height_above_anchor - main_view_->bounds().height() -
         vertical_margin;
}

bool AshNotificationView::IsMessageLabelTruncated() {
  // True if the expanded label has more than one line.
  if (message_label_in_expanded_state_->GetRequiredLines() > 1) {
    return true;
  }

  // Get the first row's width of `message_label_in_expanded_state_`'s text,
  // which is also the text width of this label since it has one line. If text
  // width is larger than `left_content()`'s width, which is the space dedicated
  // to `message_label()`, the text is truncated.
  int text_width =
      message_label_in_expanded_state_
          ->GetSubstringBounds(gfx::Range(
              0, message_label_in_expanded_state_->GetText().length()))
          .front()
          .width();
  return text_width > left_content()->width();
}

void AshNotificationView::AttachBinaryImageAsDropData(
    ui::OSExchangeData* data) {
  DCHECK(IsDraggable());

  // Fetch the original image from the large image view.
  const gfx::ImageSkia& image =
      static_cast<message_center::LargeImageView*>(
          GetViewByID(message_center::NotificationViewBase::kLargeImageView))
          ->original_image();
  DCHECK(!image.size().IsEmpty());

  // Resize `image` if necessary.
  std::optional<gfx::ImageSkia> resized_image =
      message_center_utils::ResizeImageIfExceedSizeLimit(image,
                                                         kMaxImageSizeInByte);

  // Add the drop data in the format of HTML.
  if (const std::optional<std::u16string> html_snippet = GetHtmlForBitmap(
          resized_image ? *resized_image->bitmap() : *image.bitmap())) {
    data->SetHtml(*html_snippet, /*base_url=*/GURL());
  }
}

void AshNotificationView::OnFadeOutAnimationEnded(views::View* view) {
  auto* layer = view->layer();
  if (layer) {
    layer->SetOpacity(1.0f);
  }
  view->SetVisible(false);

  if (view == image_container_view() && layer) {
    layer->SetTransform(gfx::Transform());
  }
}

base::OnceClosure AshNotificationView::OnFadeOutAnimationEndedClosure(
    views::View* view) {
  return base::BindOnce(&AshNotificationView::OnFadeOutAnimationEnded,
                        weak_factory_.GetWeakPtr(), view);
}

void AshNotificationView::OnGroupedAnimationEnded(
    views::View* left_content,
    views::View* right_content,
    views::View* message_label_in_expanded_state,
    views::View* image_container_view,
    views::View* action_buttons_row,
    AshNotificationExpandButton* expand_button,
    std::string notification_id,
    std::string parent_id) {
  auto* parent_notification =
      message_center::MessageCenter::Get()->FindNotificationById(parent_id);
  auto* child_notification =
      message_center::MessageCenter::Get()->FindNotificationById(
          notification_id);
  // The child and parent notifications are not guaranteed to exist. If
  // they were deleted avoid the animation cleanup.
  if (!parent_notification || !child_notification) {
    return;
  }

  auto* grouping_controller =
      message_center_utils::GetGroupingControllerForNotificationView(this);
  if (grouping_controller) {
    grouping_controller->ConvertFromSingleToGroupNotificationAfterAnimation(
        notification_id, parent_id, parent_notification);
  }

  left_content->layer()->SetOpacity(1.0f);
  right_content->layer()->SetOpacity(1.0f);
  message_label_in_expanded_state->layer()->SetOpacity(1.0f);
  image_container_view->layer()->SetOpacity(1.0f);
  action_buttons_row->layer()->SetOpacity(1.0f);

  // After fade out single notification and set up a group one, perform
  // a fade in.
  AnimateSingleToGroupFadeIn();

  expand_button->set_previous_bounds(expand_button->GetContentsBounds());
  DeprecatedLayoutImmediately();
  expand_button->AnimateSingleToGroupNotification();
}

base::OnceClosure AshNotificationView::OnGroupedAnimationEndedClosure(
    views::View* left_content,
    views::View* right_content,
    views::View* message_label_in_expanded_state,
    views::View* image_container_view,
    views::View* action_buttons_row,
    AshNotificationExpandButton* expand_button,
    const std::string& notification_id,
    std::string parent_id) {
  return base::BindOnce(&AshNotificationView::OnGroupedAnimationEnded,
                        weak_factory_.GetWeakPtr(), left_content, right_content,
                        message_label_in_expanded_state, image_container_view,
                        action_buttons_row, expand_button, notification_id,
                        parent_id);
}

BEGIN_METADATA(AshNotificationView)
END_METADATA

}  // namespace ash
