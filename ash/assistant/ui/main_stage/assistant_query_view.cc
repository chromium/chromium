// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_query_view.h"

#include <algorithm>
#include <memory>

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kLineHeightDip = 24;
constexpr int kMaxWidthDip = 376;
constexpr int kMinHeightDip = 32;

// Helpers ---------------------------------------------------------------------

views::StyledLabel::RangeStyleInfo CreateStyleInfo(SkColor color) {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font =
      assistant::ui::GetDefaultFontList().DeriveWithSizeDelta(2);
  style.override_color = color;
  return style;
}

}  // namespace

// AssistantQueryView ----------------------------------------------------------

AssistantQueryView::AssistantQueryView() {
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
  return std::max(views::View::GetHeightForWidth(width), kMinHeightDip);
}

void AssistantQueryView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantQueryView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  label_->SizeToFit(width());
}

void AssistantQueryView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::MAIN_AXIS_ALIGNMENT_CENTER);

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::CROSS_AXIS_ALIGNMENT_CENTER);

  // Label.
  label_ = new views::StyledLabel(base::string16(), /*listener=*/nullptr);
  label_->set_auto_color_readability_enabled(false);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);
  label_->SetLineHeight(kLineHeightDip);
  AddChildView(label_);
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
  if (high_confidence_text.empty() && low_confidence_text.empty()) {
    label_->SetText(base::string16());
  } else {
    const base::string16& high_confidence_text_16 =
        base::UTF8ToUTF16(high_confidence_text);

    const base::string16& low_confidence_text_16 =
        base::UTF8ToUTF16(low_confidence_text);

    label_->SetText(high_confidence_text_16 + low_confidence_text_16);

    // Style high confidence text.
    if (!high_confidence_text_16.empty()) {
      label_->AddStyleRange(gfx::Range(0, high_confidence_text_16.length()),
                            CreateStyleInfo(kTextColorPrimary));
    }

    // Style low confidence text.
    if (!low_confidence_text_16.empty()) {
      label_->AddStyleRange(gfx::Range(high_confidence_text_16.length(),
                                       high_confidence_text_16.length() +
                                           low_confidence_text_16.length()),
                            CreateStyleInfo(kTextColorHint));
    }
  }
  label_->SizeToFit(width());
  PreferredSizeChanged();
}

}  // namespace ash
