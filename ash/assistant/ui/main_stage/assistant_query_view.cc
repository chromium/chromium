// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include <algorithm>
#include <memory>

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kLineHeightDip = 24;
constexpr int kMaxWidthDip = 376;
constexpr int kHeightDip = 32;

// Helpers ---------------------------------------------------------------------

std::unique_ptr<views::Label> CreateLabel() {
  auto label = std::make_unique<views::Label>();
  label->SetAutoColorReadabilityEnabled(false);
  label->SetLineHeight(kLineHeightDip);
  label->SetFontList(
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(2));
  label->SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);
  return label;
}

}  // namespace

// AssistantQueryView ----------------------------------------------------------

AssistantQueryView::AssistantQueryView() {
  SetID(AssistantViewID::kQueryView);
  InitLayout();
  GetViewAccessibility().SetRole(ax::mojom::Role::kHeading);
}

AssistantQueryView::~AssistantQueryView() = default;

gfx::Size AssistantQueryView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kMaxWidthDip, kHeightDip);
}

void AssistantQueryView::OnThemeChanged() {
  views::View::OnThemeChanged();

  high_confidence_label_->SetEnabledColor(
      GetColorProvider()->GetColor(kColorAshAssistantQueryHighConfidenceLabel));
  low_confidence_label_->SetEnabledColor(
      GetColorProvider()->GetColor(kColorAshAssistantQueryLowConfidenceLabel));
}

void AssistantQueryView::InitLayout() {
  views::FlexLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal);
  layout_manager->SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  layout_manager->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // Labels
  high_confidence_label_ = AddChildView(CreateLabel());
  high_confidence_label_->SetID(AssistantViewID::kHighConfidenceLabel);
  high_confidence_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(2));

  low_confidence_label_ = AddChildView(CreateLabel());
  low_confidence_label_->SetID(AssistantViewID::kLowConfidenceLabel);
  low_confidence_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));
}

void AssistantQueryView::SetQuery(const AssistantQuery& query) {
  switch (query.type()) {
    case AssistantQueryType::kText: {
      const AssistantTextQuery& text_query =
          static_cast<const AssistantTextQuery&>(query);
      SetText(text_query.text());
      break;
    }
    case AssistantQueryType::kVoice: {
      const AssistantVoiceQuery& voice_query =
          static_cast<const AssistantVoiceQuery&>(query);
      SetText(voice_query.high_confidence_speech(),
              voice_query.low_confidence_speech());
      break;
    }
    case AssistantQueryType::kNull:
      SetText(std::string());
      break;
  }
}

void AssistantQueryView::SetText(const std::string& high_confidence_text,
                                 const std::string& low_confidence_text) {
  // When coming from the server, both |high_confidence_text| and
  // |low_confidence_text| may be HTML escaped, so we need to unescape both
  // before displaying to avoid printing HTML entities to the user.
  const std::u16string& high_confidence_text_16 =
      base::UnescapeForHTML(base::UTF8ToUTF16(high_confidence_text));

  high_confidence_label_->SetText(high_confidence_text_16);

  const std::u16string& low_confidence_text_16 =
      base::UnescapeForHTML(base::UTF8ToUTF16(low_confidence_text));

  low_confidence_label_->SetText(low_confidence_text_16);
}

BEGIN_METADATA(AssistantQueryView)
END_METADATA

}  // namespace ash
