// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_list_footer_view.h"

#include <algorithm>
#include <memory>

#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kSeeAllIconSize = 24;

constexpr int kFooterVerticalSpacing = 7;

constexpr int kFooterStartSpacing = 6;

}  // namespace

GlanceablesListFooterView::GlanceablesListFooterView(
    base::RepeatingClosure on_see_all_pressed) {
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  const auto* const typography_provider = TypographyProvider::Get();

  items_count_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetID(base::to_underlying(
              GlanceablesViewId::kListFooterItemsCountLabel))
          .SetEnabledColorId(cros_tokens::kCrosSysSecondary)
          .SetFontList(typography_provider->ResolveTypographyToken(
              TypographyToken::kCrosBody2))
          .SetLineHeight(typography_provider->ResolveLineHeight(
              TypographyToken::kCrosBody2))
          .SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT)
          .SetProperty(
              views::kFlexBehaviorKey,
              views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                       views::MaximumFlexSizeRule::kUnbounded))
          .Build());

  see_all_button_ =
      AddChildView(views::Builder<views::LabelButton>()
                       .SetText(l10n_util::GetStringUTF16(
                           IDS_GLANCEABLES_LIST_FOOTER_ACTION_BUTTON_LABEL))
                       .SetCallback(std::move(on_see_all_pressed))
                       .SetID(base::to_underlying(
                           GlanceablesViewId::kListFooterSeeAllButton))
                       .Build());
  see_all_button_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_RIGHT);
  see_all_button_->SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                     cros_tokens::kCrosSysOnSurface,
                                     kSeeAllIconSize));
  see_all_button_->SetTextColorId(views::Button::STATE_NORMAL,
                                  cros_tokens::kCrosSysOnSurface);

  SetProperty(views::kMarginsKey,
              gfx::Insets::TLBR(kFooterVerticalSpacing, kFooterStartSpacing,
                                kFooterVerticalSpacing, 0));
}

void GlanceablesListFooterView::UpdateItemsCount(size_t visible_items_count,
                                                 size_t total_items_count) {
  CHECK_LE(visible_items_count, total_items_count);
  items_count_label_->SetText(
      l10n_util::GetStringFUTF16(IDS_GLANCEABLES_LIST_FOOTER_ITEMS_COUNT_LABEL,
                                 base::NumberToString16(visible_items_count),
                                 base::NumberToString16(total_items_count)));
}

BEGIN_METADATA(GlanceablesListFooterView, views::View)
END_METADATA

}  // namespace ash
