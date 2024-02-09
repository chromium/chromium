// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_panel_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/mahi/mahi_constants.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/components/mahi/public/cpp/views/experiment_badge.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr int kPanelCornerRadius = 16;

}  // namespace

BEGIN_METADATA(MahiPanelView)
END_METADATA

MahiPanelView::MahiPanelView() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetBackground(views::CreateRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kPanelCornerRadius));

  // Create a layer for the view for background blur and rounded corners.
  SetPaintToLayer();
  layer()->SetRoundedCornerRadius(gfx::RoundedCornersF{kPanelCornerRadius});
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);
  SetBorder(std::make_unique<views::HighlightBorder>(
      kPanelCornerRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow,
      /*insets_type=*/views::HighlightBorder::InsetsType::kHalfInsets));

  auto header_row = std::make_unique<views::FlexLayoutView>();
  header_row->SetOrientation(views::LayoutOrientation::kHorizontal);

  auto header_left_container = std::make_unique<views::FlexLayoutView>();
  header_left_container->SetOrientation(views::LayoutOrientation::kHorizontal);
  header_left_container->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header_left_container->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);
  header_left_container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                   views::MaximumFlexSizeRule::kUnbounded)));

  // TODO(b/319264190): Replace the string used here with the correct string ID.
  auto header_label = std::make_unique<views::Label>(u"Mahi Panel");
  header_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
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
  close_button->SetID(ViewId::kCloseButton);
  header_row->AddChildView(std::move(close_button));

  AddChildView(std::move(header_row));

  auto* summary_label = AddChildView(std::make_unique<views::Label>());
  summary_label->SetID(ViewId::kSummaryLabel);

  auto* manager = chromeos::MahiManager::Get();
  if (manager) {
    manager->GetSummary(base::BindOnce(
        [](base::WeakPtr<MahiPanelView> parent, views::Label* summary_label,
           std::u16string summary_text) {
          if (!parent) {
            return;
          }

          summary_label->SetText(summary_text);
        },
        weak_ptr_factory_.GetWeakPtr(), summary_label));
  } else {
    CHECK_IS_TEST();
  }

  auto feedback_view = std::make_unique<views::BoxLayoutView>();
  feedback_view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  // TODO(b/319264190): Replace the string IDs used here with the correct IDs.
  auto thumbs_up_button = std::make_unique<IconButton>(
      base::BindRepeating(&MahiPanelView::OnThumbsUpButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &kMahiThumbsUpIcon,
      IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_UP);
  thumbs_up_button->SetID(ViewId::kThumbsUpButton);
  feedback_view->AddChildView(std::move(thumbs_up_button));

  auto thumbs_down_button = std::make_unique<IconButton>(
      base::BindRepeating(&MahiPanelView::OnThumbsDownButtonPressed,
                          weak_ptr_factory_.GetWeakPtr()),
      IconButton::Type::kMedium, &kMahiThumbsDownIcon,
      IDS_ASH_ACCELERATOR_DESCRIPTION_VOLUME_DOWN);
  thumbs_down_button->SetID(ViewId::kThumbsDownButton);
  feedback_view->AddChildView(std::move(thumbs_down_button));

  AddChildView(std::move(feedback_view));

  auto footer_row = std::make_unique<views::BoxLayoutView>();
  footer_row->SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  footer_row->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_DISCLAIMER_LABEL_TEXT)));

  auto learn_more_link = std::make_unique<views::Link>(
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_LEARN_MORE_LINK_LABEL_TEXT));
  learn_more_link->SetCallback(base::BindRepeating(
      &MahiPanelView::OnLearnMoreLinkClicked, weak_ptr_factory_.GetWeakPtr()));
  learn_more_link->SetID(ViewId::kLearnMoreLink);
  footer_row->AddChildView(std::move(learn_more_link));

  AddChildView(std::move(footer_row));
}

MahiPanelView::~MahiPanelView() = default;

void MahiPanelView::OnThumbsUpButtonPressed(const ui::Event& event) {
  base::UmaHistogramBoolean(mahi_constants::kMahiFeedbackHistogramName, true);
}

void MahiPanelView::OnThumbsDownButtonPressed(const ui::Event& event) {
  base::UmaHistogramBoolean(mahi_constants::kMahiFeedbackHistogramName, false);
}

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
