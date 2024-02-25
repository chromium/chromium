// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_text_element_view.h"

#include <memory>

#include "ash/assistant/model/ui/assistant_text_element.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/main_stage/assistant_ui_element_view_animator.h"
#include "ash/style/ash_color_id.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr char kAssistantTextElementHistogram[] =
    "Ash.Assistant.AnimationSmoothness.TextElement";

}  // namespace

// AssistantTextElementView ----------------------------------------------------

AssistantTextElementView::AssistantTextElementView(
    const AssistantTextElement* text_element)
    : AssistantTextElementView(text_element->text()) {}

AssistantTextElementView::AssistantTextElementView(const std::string& text) {
  InitLayout(text);
}

AssistantTextElementView::~AssistantTextElementView() = default;

ui::Layer* AssistantTextElementView::GetLayerForAnimating() {
  if (!layer()) {
    // We'll be animating this view on its own layer so we need to initialize
    // the layer for the view if we haven't done so already.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
  }
  return layer();
}

std::string AssistantTextElementView::ToStringForTesting() const {
  return base::UTF16ToUTF8(label_->GetText());
}

void AssistantTextElementView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantTextElementView::InitLayout(const std::string& text) {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // Label.
  label_ =
      AddChildView(std::make_unique<views::Label>(base::UTF8ToUTF16(text)));
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetFontList(assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(2)
                          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetMultiLine(true);
}

std::unique_ptr<ElementAnimator> AssistantTextElementView::CreateAnimator() {
  return std::make_unique<AssistantUiElementViewAnimator>(
      this, kAssistantTextElementHistogram);
}

void AssistantTextElementView::OnThemeChanged() {
  views::View::OnThemeChanged();

  label_->SetEnabledColor(
      GetColorProvider()->GetColor(kColorAshAssistantTextColorPrimary));
}

BEGIN_METADATA(AssistantTextElementView)
END_METADATA

}  // namespace ash
