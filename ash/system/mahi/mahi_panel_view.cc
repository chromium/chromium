// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <string>

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/mahi_question_answer_view.h"
#include "ash/system/mahi/summary_outlines_section.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/views/experiment_badge.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr SkScalar kContentScrollViewCornerRadius = 16;
constexpr gfx::Insets kPanelPadding = gfx::Insets(16);
constexpr int kPanelChildSpacing = 8;
constexpr int kHeaderRowSpacing = 8;
constexpr gfx::Insets kSourceRowPadding = gfx::Insets::TLBR(6, 12, 6, 14);
constexpr int kSourceRowSpacing = 8;

// Ask Question container constants.
constexpr gfx::Insets kAskQuestionContainerInteriorMargin = gfx::Insets(2);
constexpr int kAskQuestionContainerCornerRadius = 8;
constexpr int kAskQuestionContainerSpacing = 8;

// The below constants for the feedback buttons and cutout dimensions refer to
// the following spec, where an order is designated for the first, second, and
// third curves of the cutout in the content section's bottom-right corner:
// http://screen/9K4tXBZXihWN9KA.
constexpr int kFeedbackButtonIconSize = 20;
constexpr int kFeedbackButtonIconPaddingAbove = 8;
constexpr int kFeedbackButtonIconPaddingBetween = 16;
constexpr int kFeedbackButtonIconPaddingLeft = 12;

// Width of the cutout in the content section's bottom-right corner, not
// including the rounded corner immediately to its left.
constexpr int kCutoutWidth = kFeedbackButtonIconPaddingLeft +
                             kFeedbackButtonIconSize * 2 +
                             kFeedbackButtonIconPaddingBetween;
// Height of the cutout in the content section's bottom-right corner, not
// including the rounded corner immediately above it.
constexpr int kCutoutHeight =
    kFeedbackButtonIconSize + kFeedbackButtonIconPaddingAbove;

// Radius of the cutout's first and third curves.
constexpr SkScalar kCutoutConvexRadius = 10.f;
// Radius of the cutout's second curve.
constexpr SkScalar kCutoutConcaveRadius = 12.f;

// A feedback button is a "small" `IconButton`, meaning it has a button (view)
// size of 24px and an icon size of 20px. The feedback button's icon is aligned
// to the rightmost edge of the view, creating 4px of padding to the left of the
// icon. Subtract that padding from the expected space between the two icons.
// NOTE: Changes to the feedback buttons' size will affect this constant.
constexpr int kFeedbackButtonSpacing = kFeedbackButtonIconPaddingBetween - 4;

// Options for a feedback button.
enum FeedbackType {
  THUMBS_UP,
  THUMBS_DOWN,
};

// Creates a thumbs-up or thumbs-down button for the feedback section.
std::unique_ptr<IconButton> CreateFeedbackButton(FeedbackType type) {
  const bool is_thumbs_up = type == THUMBS_UP;
  auto button =
      IconButton::Builder()
          .SetCallback(base::BindRepeating(
              [](bool is_thumbs_up, const ui::Event& event) {
                base::UmaHistogramBoolean(
                    mahi_constants::kMahiFeedbackHistogramName, is_thumbs_up);
                if (!is_thumbs_up) {
                  // Open the feedback dialog if thumbs down button is pressed.
                  if (auto* const manager = chromeos::MahiManager::Get()) {
                    manager->OpenFeedbackDialog();
                  } else {
                    CHECK_IS_TEST();
                  }
                }
              },
              is_thumbs_up))
          .SetType(IconButton::Type::kSmallFloating)
          .SetVectorIcon(is_thumbs_up ? &kMahiThumbsUpIcon
                                      : &kMahiThumbsDownIcon)
          // TODO(http://b/319264190): Replace the string IDs used here with the
          // correct IDs.
          .SetAccessibleNameId(
              is_thumbs_up ? IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_UP
                           : IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_DOWN)
          .SetViewId(is_thumbs_up ? mahi_constants::ViewId::kThumbsUpButton
                                  : mahi_constants::ViewId::kThumbsDownButton)
          .Build();
  button->SetImageHorizontalAlignment(
      IconButton::HorizontalAlignment::ALIGN_RIGHT);
  button->SetImageVerticalAlignment(
      IconButton::VerticalAlignment::ALIGN_BOTTOM);
  return button;
}

// Container for scrollable content in the Mahi panel, including the summary and
// outlines section or the Q&A section. Clips its own bounds to present its
// contents within a round-cornered container with a cutout in the bottom-right.
class ContentScrollView : public views::ScrollView,
                          public views::ViewTargeterDelegate {
  METADATA_HEADER(ContentScrollView, views::ScrollView)

 public:
  ContentScrollView() {
    SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
    SetBackgroundThemeColorId(cros_tokens::kCrosSysSystemOnBase);
    ClipHeightTo(/*min_height=*/0, /*max_height=*/INT_MAX);
    SetDrawOverflowIndicator(false);
    auto scroll_bar = std::make_unique<RoundedScrollBar>(
        RoundedScrollBar::Orientation::kVertical);
    // Prevent the scroll bar from overlapping with any rounded corners or
    // extending into the cutout region.
    scroll_bar->SetInsets(gfx::Insets::TLBR(kContentScrollViewCornerRadius, 0,
                                            kCutoutHeight + kCutoutConvexRadius,
                                            0));
    scroll_bar->SetSnapBackOnDragOutside(false);
    SetVerticalScrollBar(std::move(scroll_bar));
  }

 private:
  // views::ScrollView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    const auto contents_bounds = GetContentsBounds();
    const auto width = contents_bounds.width();
    const auto height = contents_bounds.height();
    constexpr auto radius = kContentScrollViewCornerRadius;

    const auto bottom_left = SkPoint::Make(0.f, height);
    const auto top_right = SkPoint::Make(width, 0.f);
    constexpr auto top_left = SkPoint::Make(0.f, 0.f);

    // One-radius offsets that can be added to or subtracted from coordinates to
    // indicate a unidirectional move, e.g., when calculating the endpoint of an
    // arc.
    constexpr auto horizontal_offset = SkPoint::Make(radius, 0.f);
    constexpr auto vertical_offset = SkPoint::Make(0.f, radius);

    // The following spec indicates the order of the cutout's first, second, and
    // third curves: http://screen/9K4tXBZXihWN9KA.
    const auto cutout_curve1_end_x = width - kCutoutWidth;
    const auto cutout_curve1_end_y = height - kCutoutConvexRadius;

    const auto cutout_curve2_end_x = cutout_curve1_end_x + kCutoutConcaveRadius;
    const auto cutout_curve2_end_y = height - kCutoutHeight;

    const auto cutout_curve3_end_x = width;
    const auto cutout_curve3_end_y = cutout_curve2_end_y - kCutoutConvexRadius;

    auto clip_path =
        SkPathBuilder()
            // Start just after the curve of the top-left rounded corner.
            .moveTo(0.f, radius)
            // Draw the bottom-left rounded corner and a vertical line
            // connecting it to the top-left corner.
            .arcTo(bottom_left, bottom_left + horizontal_offset, radius)
            // Draw the first curve of the bottom-right corner's cutout and a
            // horizontal line connecting it to the bottom-left rounded corner.
            .arcTo(SkPoint::Make(cutout_curve1_end_x, height),
                   SkPoint::Make(cutout_curve1_end_x, cutout_curve1_end_y),
                   kCutoutConvexRadius)
            // Draw the cutout's second curve and a vertical line connecting it
            // to the first curve.
            .arcTo(SkPoint::Make(cutout_curve1_end_x, cutout_curve2_end_y),
                   SkPoint::Make(cutout_curve2_end_x, cutout_curve2_end_y),
                   kCutoutConcaveRadius)
            // Draw the cutout's third curve and a horizontal line connecting
            // it to the second curve.
            .arcTo(SkPoint::Make(cutout_curve3_end_x, cutout_curve2_end_y),
                   SkPoint::Make(cutout_curve3_end_x, cutout_curve3_end_y),
                   kCutoutConvexRadius)
            // Draw the top-right rounded corner and a vertical line connecting
            // it to the bottom-right corner's cutout.
            .arcTo(top_right, top_right - horizontal_offset, radius)
            // Draw the top-left rounded corner and a horizontal line connecting
            // it to the top-right corner.
            .arcTo(top_left, top_left + vertical_offset, radius)
            .close()
            .detach();
    SetClipPath(clip_path);
  }

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override {
    const gfx::Rect contents_bounds = GetContentsBounds();
    const gfx::Rect corner_cutout_region = gfx::Rect(
        contents_bounds.width() - kCutoutWidth,
        contents_bounds.height() - kCutoutHeight, kCutoutWidth, kCutoutHeight);
    return !rect.Intersects(corner_cutout_region);
  }
};

BEGIN_METADATA(ContentScrollView)
END_METADATA

}  // namespace

MahiPanelView::MahiPanelView() {
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);
  SetInteriorMargin(kPanelPadding);
  SetDefault(views::kMarginsKey, gfx::Insets::VH(kPanelChildSpacing, 0));
  SetIgnoreDefaultMainAxisMargins(true);
  SetCollapseMargins(true);
  SetID(mahi_constants::ViewId::kMahiPanelView);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated,
      mahi_constants::kPanelCornerRadius));

  // Create a layer for the view for background blur and rounded corners.
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF{mahi_constants::kPanelCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBorder(std::make_unique<views::HighlightBorder>(
      mahi_constants::kPanelCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow,
      /*insets_type=*/views::HighlightBorder::InsetsType::kHalfInsets));

  // Construction of the header row, which includes a back button (visible only
  // in the Q&A view), the panel title, an experiment badge and a close button.
  auto* header_row = AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetIgnoreDefaultMainAxisMargins(true)
          .SetCollapseMargins(true)
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(views::kMarginsKey,
                               gfx::Insets::VH(0, kHeaderRowSpacing));
          }))
          .Build());

  back_button_ = header_row->AddChildView(
      IconButton::Builder()
          .SetViewId(mahi_constants::ViewId::kBackButton)
          .SetType(IconButton::Type::kSmallFloating)
          .SetVisible(false)  // Visible when Q&A View is showing.
          .SetVectorIcon(&kEcheArrowBackIcon)
          .SetCallback(base::BindRepeating(&MahiPanelView::OnBackButtonPressed,
                                           weak_ptr_factory_.GetWeakPtr()))
          // TODO(b/319264190): Replace string.
          .SetAccessibleName(u"Back to summary")
          .Build());

  header_row->AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetOrientation(views::LayoutOrientation::kHorizontal)
          .SetMainAxisAlignment(views::LayoutAlignment::kStart)
          .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
          .SetIgnoreDefaultMainAxisMargins(true)
          .SetCollapseMargins(true)
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(views::kMarginsKey,
                               gfx::Insets::VH(0, kHeaderRowSpacing));
            layout->SetProperty(views::kFlexBehaviorKey,
                                views::FlexSpecification(
                                    views::MinimumFlexSizeRule::kPreferred,
                                    views::MaximumFlexSizeRule::kUnbounded));
          }))
          .AddChildren(
              views::Builder<views::Label>()
                  // TODO(b/319264190): Replace the string used here with the
                  // correct string ID.
                  .SetText(u"Mahi Panel")
                  .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                  .SetFontList(
                      TypographyProvider::Get()->ResolveTypographyToken(
                          TypographyToken::kCrosTitle1))
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurface),
              views::Builder<views::View>(
                  std::make_unique<chromeos::mahi::ExperimentBadge>()))
          .Build());

  header_row->AddChildView(
      IconButton::Builder()
          .SetViewId(mahi_constants::ViewId::kCloseButton)
          .SetType(IconButton::Type::kMediumFloating)
          .SetVectorIcon(&kMediumOrLargeCloseButtonIcon)
          // TODO(b/319264190): Replace the string used here with the
          // correct string ID.
          .SetAccessibleName(u"Close button")
          .SetCallback(base::BindRepeating(&MahiPanelView::OnCloseButtonPressed,
                                           weak_ptr_factory_.GetWeakPtr()))
          .Build());

  auto* const mahi_manager = chromeos::MahiManager::Get();

  // Add a source row containing the content icon and title.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetBackground(StyleUtil::CreateThemedFullyRoundedRectBackground(
              cros_tokens::kCrosSysSystemOnBase1))
          .SetBorder(views::CreateEmptyBorder(kSourceRowPadding))
          .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
          .SetBetweenChildSpacing(kSourceRowSpacing)
          .AddChildren(
              views::Builder<views::ImageView>()
                  .SetID(mahi_constants::kContentIcon)
                  .SetImage(ui::ImageModel::FromImageSkia(
                      mahi_manager->GetContentIcon()))
                  .SetImageSize(mahi_constants::kContentIconSize),
              views::Builder<views::Label>()
                  .SetID(mahi_constants::kContentTitle)
                  .SetText(mahi_manager->GetContentTitle())
                  .SetEnabledColorId(cros_tokens::kCrosSysOnSurfaceVariant)
                  .CustomConfigure(base::BindOnce([](views::Label* self) {
                    TypographyProvider::Get()->StyleLabel(
                        TypographyToken::kCrosAnnotation2, *self);
                  })))
          .Build());

  // Add a scrollable view of the panel's content, with a feedback section.
  AddChildView(
      views::Builder<views::View>()
          .SetID(mahi_constants::ViewId::kPanelContentsContainer)
          .SetUseDefaultFillLayout(true)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::LayoutOrientation::kVertical,
                                       views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .AddChildren(
              // Add buttons for the user to give feedback on the content.
              views::Builder<views::BoxLayoutView>()
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .SetMainAxisAlignment(
                      views::BoxLayout::MainAxisAlignment::kEnd)
                  .SetCrossAxisAlignment(
                      views::BoxLayout::CrossAxisAlignment::kEnd)
                  .SetBetweenChildSpacing(kFeedbackButtonSpacing)
                  .AddChildren(views::Builder<views::View>(
                                   CreateFeedbackButton(THUMBS_UP)),
                               views::Builder<views::View>(
                                   CreateFeedbackButton(THUMBS_DOWN))),
              views::Builder<views::ScrollView>(
                  std::make_unique<ContentScrollView>())
                  .SetContents(
                      views::Builder<views::View>()
                          .SetUseDefaultFillLayout(true)
                          .AddChildren(
                              views::Builder<SummaryOutlinesSection>()
                                  .SetID(mahi_constants::ViewId::
                                             kSummaryOutlinesSection)
                                  .CopyAddressTo(&summary_outlines_section_),
                              views::Builder<MahiQuestionAnswerView>()
                                  .SetID(mahi_constants::ViewId::
                                             kQuestionAnswerView)
                                  .CopyAddressTo(&question_answer_view_)
                                  .SetVisible(false))))
          .Build());

  auto* ask_question_container = AddChildView(
      views::Builder<views::FlexLayoutView>()
          .SetBackground(views::CreateThemedRoundedRectBackground(
              cros_tokens::kCrosSysSystemOnBase,
              gfx::RoundedCornersF(kAskQuestionContainerCornerRadius)))
          .SetInteriorMargin(kAskQuestionContainerInteriorMargin)
          .SetIgnoreDefaultMainAxisMargins(true)
          .SetCollapseMargins(true)
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(
                views::kMarginsKey,
                gfx::Insets::VH(0, kAskQuestionContainerSpacing));
          }))
          .Build());

  auto* text_field = ask_question_container->AddChildView(
      std::make_unique<SystemTextfield>(SystemTextfield::Type::kMedium));
  text_field->SetBackgroundEnabled(false);
  // TODO(b/319264190): Replace string.
  text_field->SetPlaceholderText(u"Ask a question.");
  text_field->SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
      TypographyToken::kCrosAnnotation1));
  text_field->SetTextColorId(cros_tokens::kCrosSysSecondary);
  text_field->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded,
                               /*adjust_height_for_width=*/true));

  ask_question_container->AddChildView(
      IconButton::Builder()
          .SetViewId(mahi_constants::ViewId::kAskQuestionSendButton)
          .SetType(IconButton::Type::kSmallFloating)
          .SetBackgroundColor(cros_tokens::kCrosSysSystemOnBase1)
          .SetVectorIcon(&vector_icons::kSendIcon)
          .SetCallback(base::BindRepeating(&MahiPanelView::OnSendButtonPressed,
                                           weak_ptr_factory_.GetWeakPtr()))
          // TODO(b/319264190): Replace string.
          .SetAccessibleName(u"Send")
          .Build());

  auto footer_row = std::make_unique<views::BoxLayoutView>();
  footer_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  footer_row->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_DISCLAIMER_LABEL_TEXT)));

  auto learn_more_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_LEARN_MORE_LINK_LABEL_TEXT));
  learn_more_link->SetCallback(base::BindRepeating(
      &MahiPanelView::OnLearnMoreLinkClicked, weak_ptr_factory_.GetWeakPtr()));
  learn_more_link->SetID(mahi_constants::ViewId::kLearnMoreLink);
  footer_row->AddChildView(std::move(learn_more_link));

  AddChildView(std::move(footer_row));
}

MahiPanelView::~MahiPanelView() = default;

void MahiPanelView::OnCloseButtonPressed(const ui::Event& event) {
  CHECK(GetWidget());
  GetWidget()->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
}

void MahiPanelView::OnLearnMoreLinkClicked() {
  NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(mahi_constants::kLearnMorePage),
      NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      NewWindowDelegate::Disposition::kNewForegroundTab);
}

void MahiPanelView::OnSendButtonPressed() {
  if (!question_answer_view_->GetVisible()) {
    summary_outlines_section_->SetVisible(false);
    question_answer_view_->SetVisible(true);
    back_button_->SetVisible(true);
  }
  question_answer_view_->CreateSampleQuestionAnswer();
}

void MahiPanelView::OnBackButtonPressed() {
  summary_outlines_section_->SetVisible(true);
  question_answer_view_->SetVisible(false);
  back_button_->SetVisible(false);
}

BEGIN_METADATA(MahiPanelView)
END_METADATA

}  // namespace ash
