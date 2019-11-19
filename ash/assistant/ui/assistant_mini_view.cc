// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_mini_view.h"

#include <algorithm>
#include <memory>

#include "ash/assistant/model/assistant_query.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/logo_view/logo_view.h"
#include "ash/assistant/util/assistant_util.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kIconSizeDip = 24;
constexpr int kLineHeightDip = 24;
constexpr int kMaxWidthDip = 452;
constexpr int kPaddingLeftDip = 12;
constexpr int kPaddingRightDip = 24;
constexpr int kPreferredHeightDip = 48;

}  // namespace

AssistantMiniView::AssistantMiniView(AssistantViewDelegate* delegate)
    : views::Button(this), delegate_(delegate), label_(new views::Label()) {
  InitLayout();

  // The AssistantViewDelegate should outlive AssistantMiniView.
  delegate_->AddInteractionModelObserver(this);
  delegate_->AddUiModelObserver(this);
}

AssistantMiniView::~AssistantMiniView() {
  delegate_->RemoveUiModelObserver(this);
  delegate_->RemoveInteractionModelObserver(this);
}

const char* AssistantMiniView::GetClassName() const {
  return "AssistantMiniView";
}

gfx::Size AssistantMiniView::CalculatePreferredSize() const {
  const int preferred_width =
      std::min(views::View::CalculatePreferredSize().width(), kMaxWidthDip);
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int AssistantMiniView::GetHeightForWidth(int width) const {
  return kPreferredHeightDip;
}

void AssistantMiniView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantMiniView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingLeftDip, 0, kPaddingRightDip),
          2 * kSpacingDip));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Molecule icon.
  LogoView* molecule_icon = LogoView::Create();
  molecule_icon->SetPreferredSize(gfx::Size(kIconSizeDip, kIconSizeDip));
  molecule_icon->SetState(LogoView::State::kMoleculeWavy,
                          /*animate=*/false);
  AddChildView(molecule_icon);

  // Label.
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetEnabledColor(kTextColorPrimary);
  label_->SetFontList(assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(1)
                          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  label_->SetLineHeight(kLineHeightDip);
  AddChildView(label_);

  // Initialize the prompt.
  UpdatePrompt();
}

void AssistantMiniView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  delegate_->OnMiniViewPressed();
}

void AssistantMiniView::OnInputModalityChanged(InputModality input_modality) {
  UpdatePrompt();
}

void AssistantMiniView::OnResponseChanged(
    const scoped_refptr<AssistantResponse>& response) {
  // When a response changes, the committed query becomes active. We'll cache
  // the text for that query to use as our prompt when not using the stylus.
  const AssistantQuery& committed_query =
      delegate_->GetInteractionModel()->committed_query();

  switch (committed_query.type()) {
    case AssistantQueryType::kText: {
      const AssistantTextQuery& text_query =
          static_cast<const AssistantTextQuery&>(committed_query);
      last_active_query_ = text_query.text();
      break;
    }
    case AssistantQueryType::kVoice: {
      const AssistantVoiceQuery& voice_query =
          static_cast<const AssistantVoiceQuery&>(committed_query);
      last_active_query_ = voice_query.high_confidence_speech() +
                           voice_query.low_confidence_speech();
      break;
    }
    case AssistantQueryType::kNull:
      // It shouldn't be possible to commit a query of type kNull.
      NOTREACHED();
      break;
  }

  UpdatePrompt();
}

void AssistantMiniView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    base::Optional<AssistantEntryPoint> entry_point,
    base::Optional<AssistantExitPoint> exit_point) {
  if (!assistant::util::IsFinishingSession(new_visibility))
    return;

  // When Assistant is finishing a session, we need to reset view state.
  last_active_query_.reset();
  UpdatePrompt();
}

void AssistantMiniView::UpdatePrompt() {
  InputModality input_modality =
      delegate_->GetInteractionModel()->input_modality();

  switch (input_modality) {
    case InputModality::kStylus:
      label_->SetText(
          l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_STYLUS));
      break;
    case InputModality::kKeyboard:
    case InputModality::kVoice:
      // If we've cached an active query, we'll use that as our prompt provided
      // it is non-empty. If not, we fall back to our default prompt string.
      // TODO(dmblack): Once b/112000321 is fixed we should remove empty query
      // handling as that should only occur due to an invalid interaction state.
      label_->SetText(
          last_active_query_.has_value() && !last_active_query_.value().empty()
              ? base::UTF8ToUTF16(last_active_query_.value())
              : l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));
      break;
  }
}

}  // namespace ash
