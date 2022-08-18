// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"

#include <algorithm>

#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kVersionButtonHeight = 32;
constexpr int kVersionButtonBorderRadius = 4;
constexpr int kVersionButtonImageLabelSpacing = 8;

constexpr int kVersionButtonMarginVertical = 6;
constexpr int kVersionButtonMarginHorizontal = 16;

constexpr int kVersionButtonLargeCornerRadius = 16;
constexpr int kVersionButtonSmallCornerRadius = 4;

constexpr size_t kNumVersionButtonCornerRadii = 8;
constexpr SkScalar
    kPartneredVersionButtonCorners[kNumVersionButtonCornerRadii] = {
        kVersionButtonLargeCornerRadius, kVersionButtonLargeCornerRadius,
        kVersionButtonSmallCornerRadius, kVersionButtonSmallCornerRadius,
        kVersionButtonSmallCornerRadius, kVersionButtonSmallCornerRadius,
        kVersionButtonLargeCornerRadius, kVersionButtonLargeCornerRadius};
constexpr SkScalar
    kStandaloneVersionButtonCorners[kNumVersionButtonCornerRadii] = {
        kVersionButtonLargeCornerRadius, kVersionButtonLargeCornerRadius,
        kVersionButtonLargeCornerRadius, kVersionButtonLargeCornerRadius,
        kVersionButtonLargeCornerRadius, kVersionButtonLargeCornerRadius,
        kVersionButtonLargeCornerRadius, kVersionButtonLargeCornerRadius};

constexpr int kSubmitFeedbackButtonMarginVertical = 6;
constexpr int kSubmitFeedbackButtonMarginHorizontal = 16;

constexpr int kSubmitFeedbackButtonLargeCornerRadius = 16;
constexpr int kSubmitFeedbackButtonSmallCornerRadius = 4;
constexpr SkScalar kSubmitFeedbackButtonCorners[] = {
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius};

constexpr int kButtonSpacing = 2;

// VersionButton is a base class that provides a styled button, for devices on a
// non-stable release track, that has a label for the channel and ChromeOS
// version.
class VersionButton : public views::LabelButton {
 public:
  VersionButton(version_info::Channel channel,
                const SkScalar (&corners)[kNumVersionButtonCornerRadii])
      : LabelButton(
            base::BindRepeating([] {
              Shell::Get()
                  ->system_tray_model()
                  ->client()
                  ->ShowChannelInfoAdditionalDetails();
            }),
            channel_indicator_utils::GetFullReleaseTrackString(channel)),
        channel_(channel) {
    std::copy(corners, corners + kNumVersionButtonCornerRadii, corners_);
    SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(
        kVersionButtonMarginVertical, kVersionButtonMarginHorizontal)));
    SetImageLabelSpacing(kVersionButtonImageLabelSpacing);
    SetMinSize(gfx::Size(0, kVersionButtonHeight));
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetInstallFocusRingOnFocus(true);
    views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
    views::InstallRoundRectHighlightPathGenerator(this, gfx::Insets(),
                                                  kVersionButtonBorderRadius);
  }
  VersionButton(const VersionButton&) = delete;
  VersionButton& operator=(const VersionButton&) = delete;
  ~VersionButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(channel_indicator_utils::GetBgColor(channel_));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(SkPath().addRoundRect(gfx::RectToSkRect(GetLocalBounds()),
                                           corners_, SkPathDirection::kCW),
                     flags);
  }

  void OnThemeChanged() override {
    views::LabelButton::OnThemeChanged();
    SetBackgroundAndFont();
  }

 private:
  void SetBackgroundAndFont() {
    label()->SetFontList(
        gfx::FontList().DeriveWithWeight(gfx::Font::Weight::MEDIUM));
    SetEnabledTextColors(channel_indicator_utils::GetFgColor(channel_));
  }

  // The channel itself, BETA, DEV, or CANARY.
  const version_info::Channel channel_;

  // Array of values that represents the rounded rect corners.
  SkScalar corners_[kNumVersionButtonCornerRadii];
};

// SubmitFeedbackButton provides a styled button, for devices on a
// non-stable release track, that allows the user to submit feedback.
class SubmitFeedbackButton : public IconButton {
 public:
  explicit SubmitFeedbackButton(version_info::Channel channel)
      : IconButton(base::BindRepeating([] {
                     Shell::Get()
                         ->system_tray_model()
                         ->client()
                         ->ShowChannelInfoGiveFeedback();
                   }),
                   IconButton::Type::kSmall,
                   &kRequestFeedbackIcon,
                   IDS_ASH_STATUS_TRAY_REPORT_FEEDBACK),
        channel_(channel) {
    SetBorder(views::CreateEmptyBorder(
        gfx::Insets::VH(kSubmitFeedbackButtonMarginVertical,
                        kSubmitFeedbackButtonMarginHorizontal)));
    SetIconColor(channel_indicator_utils::GetFgColor(channel_));
  }
  SubmitFeedbackButton(const SubmitFeedbackButton&) = delete;
  SubmitFeedbackButton& operator=(const SubmitFeedbackButton&) = delete;
  ~SubmitFeedbackButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setColor(channel_indicator_utils::GetBgColor(channel_));
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawPath(SkPath().addRoundRect(gfx::RectToSkRect(GetLocalBounds()),
                                           kSubmitFeedbackButtonCorners,
                                           SkPathDirection::kCW),
                     flags);
    IconButton::PaintButtonContents(canvas);
  }

 private:
  const version_info::Channel channel_;
};

}  // namespace

ChannelIndicatorQuickSettingsView::ChannelIndicatorQuickSettingsView(
    version_info::Channel channel,
    bool allow_user_feedback) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
      kUnifiedSystemInfoSpacing));
  // kCenter align the layout for this view because it is a container for the
  // buttons.
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  layout->set_between_child_spacing(kButtonSpacing);

  version_button_ = AddChildView(std::make_unique<VersionButton>(
      channel, allow_user_feedback ? kPartneredVersionButtonCorners
                                   : kStandaloneVersionButtonCorners));

  if (allow_user_feedback) {
    feedback_button_ =
        AddChildView(std::make_unique<SubmitFeedbackButton>(channel));
  }
}

bool ChannelIndicatorQuickSettingsView::IsVersionButtonVisibleForTesting() {
  return version_button_ && version_button_->GetVisible();
}

bool ChannelIndicatorQuickSettingsView::
    IsSubmitFeedbackButtonVisibleForTesting() {
  return feedback_button_ && feedback_button_->GetVisible();
}

}  // namespace ash
