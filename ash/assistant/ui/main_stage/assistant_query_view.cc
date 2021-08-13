// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include <algorithm>
#include <memory>

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/assistant/ui/colors/assistant_colors_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/color_provider.h"
#include "base/strings/utf_string_conversions.h"
#include "net/base/escape.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/chromeos/colors/cros_colors.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
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
  GetViewAccessibility().OverrideRole(ax::mojom::Role::kHeading);
}

AssistantQueryView::~AssistantQueryView() = default;

const char* AssistantQueryView::GetClassName() const {
  return "AssistantQueryView";
}

gfx::Size AssistantQueryView::CalculatePreferredSize() const {
  return gfx::Size(kMaxWidthDip, GetHeightForWidth(kMaxWidthDip));
}

int AssistantQueryView::GetHeightForWidth(int width) const {
  return kHeightDip;
}

void AssistantQueryView::OnThemeChanged() {
  views::View::OnThemeChanged();

  background()->SetNativeControlColor(ash::assistant::ResolveAssistantColor(
      assistant_colors::ColorName::kBgAssistantPlate));

  // TODO(crbug.com/1176919): We cannot use ScopedLightModeAsDefault from
  // ash/assistant/ui as it causes a circular dependency. Find a better way to
  // resolve cros_colors color.
  SkColor text_color_primary =
      features::IsDarkLightModeEnabled()
          ? ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorPrimary)
          : cros_colors::ResolveColor(cros_colors::ColorName::kTextColorPrimary,
                                      /*is_dark_mode=*/false,
                                      /*use_debug_colors=*/false);
  SkColor text_color_secondary =
      features::IsDarkLightModeEnabled()
          ? ColorProvider::Get()->GetContentLayerColor(
                ColorProvider::ContentLayerType::kTextColorSecondary)
          : cros_colors::ResolveColor(
                cros_colors::ColorName::kTextColorSecondary,
                /*is_dark_mode=*/false,
                /*use_debug_colors=*/false);

  high_confidence_label_->SetBackgroundColor(
      ash::assistant::ResolveAssistantColor(
          assistant_colors::ColorName::kBgAssistantPlate));
  high_confidence_label_->SetEnabledColor(text_color_primary);

  low_confidence_label_->SetBackgroundColor(
      ash::assistant::ResolveAssistantColor(
          assistant_colors::ColorName::kBgAssistantPlate));
  low_confidence_label_->SetEnabledColor(text_color_secondary);
}

void AssistantQueryView::InitLayout() {
  SetBackground(
      views::CreateSolidBackground(ash::assistant::ResolveAssistantColor(
          assistant_colors::ColorName::kBgAssistantPlate)));

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
      net::UnescapeForHTML(base::UTF8ToUTF16(high_confidence_text));

  high_confidence_label_->SetText(high_confidence_text_16);

  const std::u16string& low_confidence_text_16 =
      net::UnescapeForHTML(base::UTF8ToUTF16(low_confidence_text));

  low_confidence_label_->SetText(low_confidence_text_16);
}

}  // namespace ash
