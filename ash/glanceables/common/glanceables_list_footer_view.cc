// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_list_footer_view.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/glanceables/common/glanceables_view_id.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/typography.h"
#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

constexpr int kSeeAllIconLabelSpacing = 6;

class SeeAllButton : public views::LabelButton {
  METADATA_HEADER(SeeAllButton, views::LabelButton)

 public:
  explicit SeeAllButton(base::RepeatingClosure on_see_all_pressed) {
    const bool stable_launch =
        features::AreAnyGlanceablesTimeManagementViewsEnabled();
    SetText(stable_launch
                ? u""
                : l10n_util::GetStringUTF16(
                      IDS_GLANCEABLES_LIST_FOOTER_ACTION_BUTTON_LABEL));
    // TODO(b/333770880): Revisit this to see if it can be refactored.
    if (stable_launch) {
      // Explicitly set an empty border to replace the border created by default
      // in LabelButton.
      SetBorder(views::CreateEmptyBorder(0));
    } else {
      SetBorder(views::CreateEmptyBorder(gfx::Insets::VH(2, 2)));
    }

    SetCallback(std::move(on_see_all_pressed));
    SetID(base::to_underlying(GlanceablesViewId::kListFooterSeeAllButton));
    SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_RIGHT);
    SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(vector_icons::kLaunchIcon,
                                       cros_tokens::kCrosSysOnSurface));
    SetImageLabelSpacing(kSeeAllIconLabelSpacing);
    SetTextColorId(views::Button::STATE_NORMAL, cros_tokens::kCrosSysOnSurface);
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosButton2,
                                          *label());
    views::FocusRing::Get(this)->SetColorId(cros_tokens::kCrosSysFocusRing);
  }

  SeeAllButton(const SeeAllButton&) = delete;
  SeeAllButton& operator=(const SeeAllButton&) = delete;
  ~SeeAllButton() override = default;
};

BEGIN_METADATA(SeeAllButton)
END_METADATA

}  // namespace

GlanceablesListFooterView::GlanceablesListFooterView(
    base::RepeatingClosure on_see_all_pressed) {
  SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  const auto* const typography_provider = TypographyProvider::Get();
  title_label_ = AddChildView(
      views::Builder<views::Label>()
          .SetID(base::to_underlying(GlanceablesViewId::kListFooterTitleLabel))
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
      AddChildView(std::make_unique<SeeAllButton>(on_see_all_pressed));
}

void GlanceablesListFooterView::SetTitleText(const std::u16string& title_text) {
  title_label_->SetText(title_text);
}

void GlanceablesListFooterView::SetSeeAllAccessibleName(
    const std::u16string& see_all_accessible_name) {
  see_all_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kLink);
  see_all_button_->GetViewAccessibility().SetName(see_all_accessible_name);
}

BEGIN_METADATA(GlanceablesListFooterView)
END_METADATA

}  // namespace ash
