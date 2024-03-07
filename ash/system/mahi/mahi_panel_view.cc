// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <algorithm>
#include <climits>
#include <memory>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/summary_outlines_section.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/views/experiment_badge.h"
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
          .SetType(IconButton::Type::kSmall)
          .SetVectorIcon(is_thumbs_up ? &kMahiThumbsUpIcon
                                      : &kMahiThumbsDownIcon)
          // TODO(http://b/319264190): Replace the string IDs used here with the
          // correct IDs.
          .SetAccessibleNameId(
              is_thumbs_up ? IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_UP
                           : IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_DOWN)
          .SetViewId(is_thumbs_up ? mahi_constants::ViewId::kThumbsUpButton
                                  : mahi_constants::ViewId::kThumbsDownButton)
          .SetBackgroundColor(cros_tokens::kCrosSysSystemBaseElevated)
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
class ContentScrollView : public views::ScrollView {
  METADATA_HEADER(ContentScrollView, views::ScrollView)

 public:
  ContentScrollView() {
    SetBackgroundThemeColorId(cros_tokens::kCrosSysSystemOnBase);
    SetProperty(views::kFlexBehaviorKey,
                views::FlexSpecification(views::FlexSpecification(
                    views::MinimumFlexSizeRule::kPreferred,
                    views::MaximumFlexSizeRule::kUnbounded)));
    ClipHeightTo(/*min_height=*/0, /*max_height=*/INT_MAX);
    SetDrawOverflowIndicator(false);
    SetContents(views::Builder<views::View>()
                    .SetUseDefaultFillLayout(true)
                    .AddChild(views::Builder<SummaryOutlinesSection>())
                    // TODO(htpp://b/319731486): Change this view to the Q&A
                    // section and show/hide it accordingly.
                    .AddChild(views::Builder<views::View>())
                    .Build());
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

    // The below constants for the feedback buttons and cutout dimensions refer
    // to the following spec, where I also designate an order for the cutout's
    // "first", "second", and "third" curves: http://screen/9K4tXBZXihWN9KA.

    constexpr auto feedback_button_size = 20;
    constexpr auto feedback_button_padding_above = 8;
    constexpr auto feedback_button_padding_between = 16;
    constexpr auto feedback_button_padding_left = 12;

    // Radius of the cutout's first and third curves.
    constexpr auto cutout_convex_radius = 10.f;
    // Radius of the cutout's second curve.
    constexpr auto cutout_concave_radius = 12.f;

    // Width of the cutout in the bottom-right corner, not including the rounded
    // corner immediately to its left.
    const auto cutout_width = feedback_button_padding_left +
                              feedback_button_size * 2 +
                              feedback_button_padding_between;
    // Height of the cutout in the bottom-right corner, not including the
    // rounded corner immediately above it.
    const auto cutout_height =
        feedback_button_size + feedback_button_padding_above;

    const auto cutout_curve1_end_x = width - cutout_width;
    const auto cutout_curve1_end_y = height - cutout_convex_radius;

    const auto cutout_curve2_end_x =
        cutout_curve1_end_x + cutout_concave_radius;
    const auto cutout_curve2_end_y = height - cutout_height;

    const auto cutout_curve3_end_x = width;
    const auto cutout_curve3_end_y = cutout_curve2_end_y - cutout_convex_radius;

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
                   cutout_convex_radius)
            // Draw the cutout's second curve and a vertical line connecting it
            // to the first curve.
            .arcTo(SkPoint::Make(cutout_curve1_end_x, cutout_curve2_end_y),
                   SkPoint::Make(cutout_curve2_end_x, cutout_curve2_end_y),
                   cutout_concave_radius)
            // Draw the cutout's third curve and a horizontal line connecting
            // it to the second curve.
            .arcTo(SkPoint::Make(cutout_curve3_end_x, cutout_curve2_end_y),
                   SkPoint::Make(cutout_curve3_end_x, cutout_curve3_end_y),
                   cutout_convex_radius)
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
};

BEGIN_METADATA(ContentScrollView)
END_METADATA

}  // namespace

BEGIN_METADATA(MahiPanelView)
END_METADATA

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

  // Views constructions.
  auto header_row = std::make_unique<views::FlexLayoutView>();
  header_row->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto header_left_container = std::make_unique<views::FlexLayoutView>();
  header_left_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_left_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_left_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_left_container->SetDefault(
      views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, kHeaderRowSpacing));
  header_left_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)));

  // TODO(b/319264190): Replace the string used here with the correct string ID.
  auto header_label = std::make_unique<views::Label>(u"Mahi Panel");
  header_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosTitle1,
                                        *header_label);
  header_label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  header_left_container->AddChildView(std::move(header_label));

  header_left_container->AddChildView(
      std::make_unique<chromeos::mahi::ExperimentBadge>());

  header_row->AddChildView(std::move(header_left_container));

  // TODO(b/319264190): Replace the string IDs used here with the correct IDs.
  auto close_button = std::make_unique<IconButton>(
      base::BindRepeating(&MahiPanelView::OnCloseButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &kMediumOrLargeCloseButtonIcon,
      IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_DOWN);
  close_button->SetID(mahi_constants::ViewId::kCloseButton);
  header_row->AddChildView(std::move(close_button));

  AddChildView(std::move(header_row));

  auto* const mahi_manager = chromeos::MahiManager::Get();

  // Add the content icon and title.
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

  // Scrollable contents, which should contain the summary and outlines section
  // and the Q&A section.
  AddChildView(std::make_unique<ContentScrollView>());

  auto feedback_view = std::make_unique<views::BoxLayoutView>();
  feedback_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  feedback_view->AddChildView(CreateFeedbackButton(THUMBS_UP));
  feedback_view->AddChildView(CreateFeedbackButton(THUMBS_DOWN));
  AddChildView(std::move(feedback_view));

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

}  // namespace ash
