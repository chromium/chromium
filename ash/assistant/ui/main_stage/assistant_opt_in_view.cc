// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Appearance.
constexpr int kPreferredHeightDip = 32;

// Helpers ---------------------------------------------------------------------

views::StyledLabel::RangeStyleInfo CreateStyleInfo(
    gfx::Font::Weight weight = gfx::Font::Weight::NORMAL) {
  views::StyledLabel::RangeStyleInfo style;
  style.custom_font = assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(2)
                          .DeriveWithWeight(weight);
  style.override_color = SK_ColorWHITE;
  return style;
}

std::u16string GetAction(int consent_status) {
  return consent_status == assistant::prefs::ConsentStatus::kUnauthorized
             ? l10n_util::GetStringUTF16(
                   IDS_ASH_ASSISTANT_OPT_IN_ASK_ADMINISTRATOR)
             : l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_OPT_IN_GET_STARTED);
}

// AssistantOptInContainer -----------------------------------------------------

class AssistantOptInContainer : public views::Button {
  METADATA_HEADER(AssistantOptInContainer, views::Button)

 public:
  explicit AssistantOptInContainer(views::Button::PressedCallback callback)
      : views::Button(std::move(callback)) {
    constexpr float kHighlightOpacity = 0.06f;
    SetFocusPainter(views::Painter::CreateSolidRoundRectPainter(
        SkColorSetA(SK_ColorBLACK, 0xff * kHighlightOpacity),
        kPreferredHeightDip / 2));
  }

  AssistantOptInContainer(const AssistantOptInContainer&) = delete;

  AssistantOptInContainer& operator=(const AssistantOptInContainer) = delete;

  ~AssistantOptInContainer() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    const int preferred_width =
        views::View::CalculatePreferredSize(available_size).width();
    return gfx::Size(preferred_width, kPreferredHeightDip);
  }

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void OnPaintBackground(gfx::Canvas* canvas) override {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(gfx::kGoogleBlue500);
    canvas->DrawRoundRect(GetContentsBounds(), height() / 2, flags);
  }
};

BEGIN_METADATA(AssistantOptInContainer)
END_METADATA

}  // namespace

// AssistantOptInView ----------------------------------------------------------

AssistantOptInView::AssistantOptInView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kOptInView);
  InitLayout();
  AssistantState::Get()->AddObserver(this);
}

AssistantOptInView::~AssistantOptInView() {
  AssistantState::Get()->RemoveObserver(this);
}

void AssistantOptInView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantOptInView::OnAssistantConsentStatusChanged(int consent_status) {
  UpdateLabel(consent_status);
}

void AssistantOptInView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Container.
  container_ = AddChildView(
      std::make_unique<AssistantOptInContainer>(base::BindRepeating(
          &AssistantOptInView::OnButtonPressed, base::Unretained(this))));

  layout_manager =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets::VH(0, assistant::ui::kHorizontalPadding)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Label.
  label_ = container_->AddChildView(std::make_unique<views::StyledLabel>());
  label_->SetID(AssistantViewID::kOptInViewStyledLabel);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  UpdateLabel(AssistantState::Get()->consent_status().value_or(
      assistant::prefs::ConsentStatus::kUnknown));
}

void AssistantOptInView::UpdateLabel(int consent_status) {
  // First substitution string: "Unlock more Assistant features."
  const std::u16string unlock_features =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_OPT_IN_UNLOCK_MORE_FEATURES);

  // Second substitution string specifies the action to be taken.
  const std::u16string action = GetAction(consent_status);

  // Set the text, having replaced placeholders in the opt in prompt with
  // substitution strings and caching their offset positions for styling.
  std::vector<size_t> offsets;
  auto label_text = l10n_util::GetStringFUTF16(
      IDS_ASH_ASSISTANT_OPT_IN_PROMPT, unlock_features, action, &offsets);
  label_->SetText(label_text);

  // Style the first substitution string.
  label_->AddStyleRange(
      gfx::Range(offsets.at(0), offsets.at(0) + unlock_features.length()),
      CreateStyleInfo());

  // Style the second substitution string.
  label_->AddStyleRange(
      gfx::Range(offsets.at(1), offsets.at(1) + action.length()),
      CreateStyleInfo(gfx::Font::Weight::BOLD));

  container_->GetViewAccessibility().SetName(label_text);

  // After updating the |label_| we need to ensure that it is remeasured and
  // repainted to address a timing bug in which the AssistantOptInView was
  // sometimes drawn in an invalid state (b/130758812).
  container_->DeprecatedLayoutImmediately();
  container_->SchedulePaint();
}

void AssistantOptInView::OnButtonPressed() {
  delegate_->OnOptInButtonPressed();
}

BEGIN_METADATA(AssistantOptInView)
END_METADATA

}  // namespace ash
