// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/summary_outlines_section.h"

#include <memory>
#include <string>

#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_animation_utils.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/mahi/resources/grit/mahi_resources.h"
#include "base/check_is_test.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/controls/animated_image_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int64_t kSectionHeaderChildSpacing = 4;
constexpr int64_t kSectionHeaderIconSize = 20;
constexpr gfx::Insets kSectionPadding = gfx::Insets::TLBR(8, 8, 16, 8);
constexpr int64_t kSectionChildSpacing = 8;

std::unique_ptr<views::View> CreateSectionHeader(const gfx::VectorIcon& icon,
                                                 int name_id) {
  auto view = std::make_unique<views::BoxLayoutView>();
  view->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  view->SetBetweenChildSpacing(kSectionHeaderChildSpacing);

  view->AddChildView(
      std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
          icon, cros_tokens::kCrosSysOnSurface, kSectionHeaderIconSize)));

  auto label =
      std::make_unique<views::Label>(l10n_util::GetStringUTF16(name_id));
  label->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);
  TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2, *label);
  view->AddChildView(std::move(label));
  return view;
}

}  // namespace

SummaryOutlinesSection::SummaryOutlinesSection() {
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  SetInsideBorderInsets(kSectionPadding);
  SetBetweenChildSpacing(kSectionChildSpacing);

  AddChildView(CreateSectionHeader(chromeos::kMahiSummarizeIcon,
                                   IDS_MAHI_PANEL_SUMMARY_SECTION_NAME));

  AddChildView(
      views::Builder<views::AnimatedImageView>()
          .CopyAddressTo(&summary_loading_animated_image_)
          .SetID(mahi_constants::ViewId::kSummaryLoadingAnimatedImage)
          .SetAnimatedImage(mahi_animation_utils::GetLottieAnimationData(
              IDR_MAHI_LOADING_SUMMARY_ANIMATION))
          .Build());

  AddChildView(views::Builder<views::Label>()
                   .CopyAddressTo(&summary_label_)
                   .SetVisible(false)
                   .SetID(mahi_constants::ViewId::kSummaryLabel)
                   .SetMultiLine(true)
                   .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
                   .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
                   .AfterBuild(base::BindOnce([](views::Label* self) {
                     TypographyProvider::Get()->StyleLabel(
                         TypographyToken::kCrosBody2, *self);
                   }))
                   .Build());

  AddChildView(CreateSectionHeader(chromeos::kMahiOutlinesIcon,
                                   IDS_MAHI_PANEL_OUTLINES_SECTION_NAME));

  AddChildView(
      views::Builder<views::AnimatedImageView>()
          .CopyAddressTo(&outlines_loading_animated_image_)
          .SetID(mahi_constants::ViewId::kOutlinesLoadingAnimatedImage)
          .SetAnimatedImage(mahi_animation_utils::GetLottieAnimationData(
              IDR_MAHI_LOADING_OUTLINES_ANIMATION))
          .Build());

  AddChildView(views::Builder<views::FlexLayoutView>()
                   .SetID(mahi_constants::ViewId::kOutlinesContainer)
                   .SetOrientation(views::LayoutOrientation::kVertical)
                   .SetVisible(false)
                   .Build());

  LoadSummaryAndOutlines();
}

SummaryOutlinesSection::~SummaryOutlinesSection() = default;

void SummaryOutlinesSection::LoadSummaryAndOutlines() {
  auto* manager = chromeos::MahiManager::Get();
  if (!manager) {
    CHECK_IS_TEST();
    return;
  }

  manager->GetSummary(base::BindOnce(&SummaryOutlinesSection::OnSummaryLoaded,
                                     weak_ptr_factory_.GetWeakPtr()));

  summary_loading_animated_image_->Play(
      mahi_animation_utils::GetLottiePlaybackConfig(
          *summary_loading_animated_image_->animated_image()->skottie(),
          IDR_MAHI_LOADING_SUMMARY_ANIMATION));

  manager->GetOutlines(base::BindOnce(&SummaryOutlinesSection::OnOutlinesLoaded,
                                      weak_ptr_factory_.GetWeakPtr()));
  outlines_loading_animated_image_->Play(
      mahi_animation_utils::GetLottiePlaybackConfig(
          *outlines_loading_animated_image_->animated_image()->skottie(),
          IDR_MAHI_LOADING_OUTLINES_ANIMATION));
}

void SummaryOutlinesSection::OnSummaryLoaded(
    std::u16string summary_text,
    chromeos::MahiResponseStatus status) {
  summary_label_->SetVisible(true);
  summary_label_->SetText(summary_text);
  summary_loading_animated_image_->Stop();
  summary_loading_animated_image_->SetVisible(false);
}

void SummaryOutlinesSection::OnOutlinesLoaded(
    std::vector<chromeos::MahiOutline> outlines,
    chromeos::MahiResponseStatus status) {
  auto* outlines_container =
      GetViewByID(mahi_constants::ViewId::kOutlinesContainer);
  for (auto outline : outlines) {
    outlines_container->AddChildView(
        views::Builder<views::Label>()
            .SetText(outline.outline_content)
            .SetMultiLine(true)
            .SetEnabledColorId(cros_tokens::kCrosSysOnSurface)
            .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
            .AfterBuild(base::BindOnce([](views::Label* self) {
              TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosBody2,
                                                    *self);
            }))
            .Build());
  }

  outlines_loading_animated_image_->Stop();
  outlines_loading_animated_image_->SetVisible(false);
  outlines_container->SetVisible(true);
}

BEGIN_METADATA(SummaryOutlinesSection)
END_METADATA

}  // namespace ash
