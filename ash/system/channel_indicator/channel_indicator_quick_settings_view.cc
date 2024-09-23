// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "base/check.h"
#include "base/i18n/rtl.h"
#include "base/ranges/algorithm.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

constexpr int kVersionButtonHeight = 32;
constexpr int kVersionButtonLargeCornerRadius = 16;
constexpr int kVersionButtonSmallCornerRadius = 4;
constexpr int kSubmitFeedbackButtonMarginTop = 5;
constexpr int kSubmitFeedbackButtonMarginBottom = 3;
constexpr int kSubmitFeedbackButtonMarginLeft = 6;
constexpr int kSubmitFeedbackButtonMarginRight = 8;
constexpr int kSubmitFeedbackButtonLargeCornerRadius = 16;
constexpr int kSubmitFeedbackButtonSmallCornerRadius = 4;
constexpr int kSubmitFeedbackButtonHeight = 32;
constexpr int kSubmitFeedbackButtonWidth = 40;
constexpr int kSubmitFeedbackButtonIconSize = 20;
constexpr int kButtonSpacing = 6;
constexpr float kVersionButtonStrokeWidth = 1.0f;

// Corners for the `VersionButton` contents. If it's shown alongside its
// "partner" (the `SubmitFeedbackButton`) then only one side is rounded,
// otherwise both sides are rounded. Calling
// `SetFlipCanvasOnPaintForRTLUI(true)` for the view means only one set of
// corners for the "partnered" case is needed for both RTL and LTR.
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

// Corners for the `VersionButton` ink drop. For this, the "partnered" case
// requires separate sets of corners for RTL and LTR.
constexpr gfx::RoundedCornersF kPartneredVersionButtonInkDropCornersLToR(
    kVersionButtonLargeCornerRadius,
    kVersionButtonSmallCornerRadius,
    kVersionButtonSmallCornerRadius,
    kVersionButtonLargeCornerRadius);
constexpr gfx::RoundedCornersF kPartneredVersionButtonInkDropCornersRToL(
    kVersionButtonSmallCornerRadius,
    kVersionButtonLargeCornerRadius,
    kVersionButtonLargeCornerRadius,
    kVersionButtonSmallCornerRadius);
constexpr gfx::RoundedCornersF kStandaloneVersionButtonInkDropCorners(
    kVersionButtonLargeCornerRadius,
    kVersionButtonLargeCornerRadius,
    kVersionButtonLargeCornerRadius,
    kVersionButtonLargeCornerRadius);

// Corners for the `SubmitFeedbackButton` contents.
constexpr SkScalar kSubmitFeedbackButtonCorners[] = {
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius};

// Corners for the `SubmitFeedbackButton` ink drop. For this, the "partnered"
// case requires separate sets of corners for RTL and LTR.
constexpr gfx::RoundedCornersF kSubmitFeedbackButtonInkDropCornersLToR(
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius);
constexpr gfx::RoundedCornersF kSubmitFeedbackButtonInkDropCornersRToL(
    kSubmitFeedbackButtonLargeCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonSmallCornerRadius,
    kSubmitFeedbackButtonLargeCornerRadius);

// Returns an array of `SkScalar` used to generate the rounded rect that's
// painted for the button, the same regardless of RTL/LTR but may be different
// if `VersionButton` is "standalone" vs. "partnered" with a
// `SubmitFeedbackButton`.
const SkScalar (&GetVersionButtonContentCorners(
    bool allow_user_feedback))[kNumVersionButtonCornerRadii] {
  return allow_user_feedback ? kPartneredVersionButtonCorners
                             : kStandaloneVersionButtonCorners;
}

// Returns a `gfx::RoundedCornersF` used to generate the highlight path and ink
// drop, will be different depending on RTL/LTR.
const gfx::RoundedCornersF& GetVersionButtonInkDropCorners(
    bool allow_user_feedback) {
  return allow_user_feedback ? base::i18n::IsRTL()
                                   ? kPartneredVersionButtonInkDropCornersRToL
                                   : kPartneredVersionButtonInkDropCornersLToR
                             : kStandaloneVersionButtonInkDropCorners;
}

const gfx::RoundedCornersF& GetSubmitFeedbackButtonInkDropCorners() {
  return base::i18n::IsRTL() ? kSubmitFeedbackButtonInkDropCornersRToL
                             : kSubmitFeedbackButtonInkDropCornersLToR;
}

// VersionButton is a base class that provides a styled button, for devices on a
// non-stable release track, that has a label for the channel and ChromeOS
// version.
class VersionButton : public views::LabelButton {
  METADATA_HEADER(VersionButton, views::LabelButton)

 public:
  VersionButton(version_info::Channel channel, bool allow_user_feedback)
      : LabelButton(
            base::BindRepeating([](const ui::Event& event) {
              // Do nothing if it's shown on non-logged-in screen.
              if (Shell::Get()->session_controller()->GetSessionState() !=
                  session_manager::SessionState::ACTIVE) {
                return;
              }
              quick_settings_metrics_util::RecordQsButtonActivated(
                  QsButtonCatalogName::kVersionButton);
              Shell::Get()
                  ->system_tray_model()
                  ->client()
                  ->ShowChannelInfoAdditionalDetails();
            }),
            channel_indicator_utils::GetFullReleaseTrackString(channel)),
        allow_user_feedback_(allow_user_feedback) {
    SetID(VIEW_ID_QS_VERSION_BUTTON);
    SetFlipCanvasOnPaintForRTLUI(true);
    const auto& content_corners =
        GetVersionButtonContentCorners(allow_user_feedback);
    base::ranges::copy(content_corners, content_corners_);
    SetHorizontalAlignment(gfx::ALIGN_CENTER);
    SetMinSize(gfx::Size(0, kVersionButtonHeight));

    StyleUtil::InstallRoundedCornerHighlightPathGenerator(
        this, GetVersionButtonInkDropCorners(allow_user_feedback));
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);

    // The button is not focusable and with no clickable effect if it's shown on
    // non-logged-in screen.
    if (Shell::Get()->session_controller()->GetSessionState() !=
        session_manager::SessionState::ACTIVE) {
      SetFocusBehavior(FocusBehavior::NEVER);
      return;
    }
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  }
  VersionButton(const VersionButton&) = delete;
  VersionButton& operator=(const VersionButton&) = delete;
  ~VersionButton() override = default;

  void SetNarrowLayout(bool narrow) {
    if (allow_user_feedback_ && !narrow) {
      // Visually center the label by adding an empty border on the left side
      // that is the same width as the feedback button on the right.
      SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
          0, kButtonSpacing + kSubmitFeedbackButtonWidth, 0, 0)));
    } else {
      // No special centering.
      SetBorder(nullptr);
    }
  }

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    gfx::RectF bounds(GetLocalBounds());
    flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSeparator));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    const float half_stroke_width = kVersionButtonStrokeWidth / 2.0f;
    bounds.Inset(half_stroke_width);

    flags.setAntiAlias(true);
    flags.setStrokeWidth(kVersionButtonStrokeWidth);
    canvas->DrawPath(
        SkPath().addRoundRect(gfx::RectFToSkRect(bounds), content_corners_),
        flags);
  }

  void OnThemeChanged() override {
    views::LabelButton::OnThemeChanged();
    views::InkDrop::Get(this)->SetBaseColor(
        GetColorProvider()->GetColor(kColorAshInkDropOpaqueColor));
    SetBackgroundAndFont();
  }

 private:
  void SetBackgroundAndFont() {
    SetEnabledTextColorIds(cros_tokens::kCrosSysOnSurfaceVariant);
    label()->SetFontList(ash::TypographyProvider::Get()->ResolveTypographyToken(
        ash::TypographyToken::kCrosBody2));
    label()->SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(0, 6)));
  }

  // Whether the user is allowed to send feedback.
  const bool allow_user_feedback_;

  // Array of values that represents the content rounded rect corners.
  SkScalar content_corners_[kNumVersionButtonCornerRadii];
};

BEGIN_METADATA(VersionButton)
END_METADATA

// SubmitFeedbackButton provides a styled button, for devices on a
// non-stable release track, that allows the user to submit feedback.
class SubmitFeedbackButton : public IconButton {
  METADATA_HEADER(SubmitFeedbackButton, IconButton)

 public:
  // `content_corners` - an array of `SkScalar` used to generate the rounded
  // rect that's painted for the button, the same regardless of RTL/LTR.
  // `highlight_corners` - a `gfx::RoundedCornersF` used to generate the
  // highlight path and ink drop, will be different depending on RTL/LTR.
  explicit SubmitFeedbackButton(
      version_info::Channel channel,
      const SkScalar (&content_corners)[kNumVersionButtonCornerRadii],
      const gfx::RoundedCornersF& highlight_corners)
      : IconButton(base::BindRepeating([](const ui::Event& event) {
                     quick_settings_metrics_util::RecordQsButtonActivated(
                         QsButtonCatalogName::kFeedBackButton);
                     Shell::Get()
                         ->system_tray_model()
                         ->client()
                         ->ShowChannelInfoGiveFeedback();
                   }),
                   IconButton::Type::kMediumFloating,
                   &kRequestFeedbackIcon,
                   IDS_ASH_STATUS_TRAY_REPORT_FEEDBACK) {
    SetID(VIEW_ID_QS_FEEDBACK_BUTTON);
    base::ranges::copy(content_corners, content_corners_);
    SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
        kSubmitFeedbackButtonMarginTop, kSubmitFeedbackButtonMarginLeft,
        kSubmitFeedbackButtonMarginBottom, kSubmitFeedbackButtonMarginRight)));
    SetIconSize(kSubmitFeedbackButtonIconSize);
    SetPreferredSize(
        gfx::Size(kSubmitFeedbackButtonWidth, kSubmitFeedbackButtonHeight));

    // Icon colors are set in OnThemeChanged().
    views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
    StyleUtil::InstallRoundedCornerHighlightPathGenerator(this,
                                                          highlight_corners);
  }
  SubmitFeedbackButton(const SubmitFeedbackButton&) = delete;
  SubmitFeedbackButton& operator=(const SubmitFeedbackButton&) = delete;
  ~SubmitFeedbackButton() override = default;

  // views::LabelButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    gfx::RectF bounds(GetLocalBounds());
    flags.setColor(
        GetColorProvider()->GetColor(cros_tokens::kCrosSysSeparator));
    flags.setStyle(cc::PaintFlags::kStroke_Style);
    const float half_stroke_width = kVersionButtonStrokeWidth / 2.0f;
    bounds.Inset(half_stroke_width);

    flags.setAntiAlias(true);
    flags.setStrokeWidth(kVersionButtonStrokeWidth);
    canvas->DrawPath(
        SkPath().addRoundRect(gfx::RectFToSkRect(bounds), content_corners_),
        flags);
    IconButton::PaintButtonContents(canvas);
  }

  void OnThemeChanged() override {
    auto* color_provider = GetColorProvider();
    SetIconColor(cros_tokens::kCrosSysOnSurfaceVariant);

    const SkColor ink_drop_base_color =
        color_provider->GetColor(kColorAshInkDropOpaqueColor);
    // Enable ink drop on hover.
    StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                     /*highlight_on_hover=*/true,
                                     /*highlight_on_focus=*/false,
                                     ink_drop_base_color);
    views::InkDrop::Get(this)->SetBaseColor(ink_drop_base_color);

    IconButton::OnThemeChanged();
  }

 private:
  // Array of values that represents the content rounded rect corners.
  SkScalar content_corners_[kNumVersionButtonCornerRadii];
};

BEGIN_METADATA(SubmitFeedbackButton)
END_METADATA

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

  version_button_ = AddChildView(
      std::make_unique<VersionButton>(channel, allow_user_feedback));

  // Stretch version button horizontally.
  layout->SetFlexForView(version_button_, 1);

  if (allow_user_feedback) {
    feedback_button_ = AddChildView(std::make_unique<SubmitFeedbackButton>(
        channel, kSubmitFeedbackButtonCorners,
        GetSubmitFeedbackButtonInkDropCorners()));
  }
}

void ChannelIndicatorQuickSettingsView::SetNarrowLayout(bool narrow) {
  DCHECK(views::IsViewClass<VersionButton>(version_button_));
  views::AsViewClass<VersionButton>(version_button_)->SetNarrowLayout(narrow);
}

BEGIN_METADATA(ChannelIndicatorQuickSettingsView)
END_METADATA

}  // namespace ash
