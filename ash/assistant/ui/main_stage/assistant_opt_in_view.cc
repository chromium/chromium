// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_opt_in_view.h"

#include <memory>
#include <vector>

#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
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

base::string16 GetAction(int consent_status) {
  return consent_status ==
                 chromeos::assistant::prefs::ConsentStatus::kUnauthorized
             ? l10n_util::GetStringUTF16(
                   IDS_ASH_ASSISTANT_OPT_IN_ASK_ADMINISTRATOR)
             : l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_OPT_IN_GET_STARTED);
}

// AssistantOptInContainer -----------------------------------------------------

class AssistantOptInContainer : public views::Button {
 public:
  explicit AssistantOptInContainer(views::ButtonListener* listener)
      : views::Button(listener) {
    constexpr float kHighlightOpacity = 0.06f;
    SetFocusPainter(views::Painter::CreateSolidRoundRectPainter(
        SkColorSetA(SK_ColorBLACK, 0xff * kHighlightOpacity),
        kPreferredHeightDip / 2));
  }

  ~AssistantOptInContainer() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize() const override {
    const int preferred_width = views::View::CalculatePreferredSize().width();
    return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
  }

  int GetHeightForWidth(int width) const override {
    return kPreferredHeightDip;
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

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantOptInContainer);
};

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

const char* AssistantOptInView::GetClassName() const {
  return "AssistantOptInView";
}

void AssistantOptInView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantOptInView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  label_->SizeToFit(width());
}

void AssistantOptInView::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  delegate_->OnOptInButtonPressed();
}

void AssistantOptInView::OnAssistantConsentStatusChanged(int consent_status) {
  UpdateLabel(consent_status);
}

void AssistantOptInView::InitLayout() {
  views::BoxLayout* layout_manager =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));

  layout_manager->set_cross_axis_alignment(
      app_list_features::IsAssistantLauncherUIEnabled()
          ? views::BoxLayout::CrossAxisAlignment::kCenter
          : views::BoxLayout::CrossAxisAlignment::kEnd);

  layout_manager->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  // Container.
  container_ = new AssistantOptInContainer(/*listener=*/this);

  layout_manager =
      container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal,
          gfx::Insets(0, kPaddingDip)));

  layout_manager->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  AddChildView(container_);

  // Label.
  label_ = new views::StyledLabel(base::string16(), /*listener=*/nullptr);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  container_->AddChildView(label_);
  container_->SetFocusForPlatform();

  UpdateLabel(AssistantState::Get()->consent_status().value_or(
      chromeos::assistant::prefs::ConsentStatus::kUnknown));
}

void AssistantOptInView::UpdateLabel(int consent_status) {
  // First substitution string: "Unlock more Assistant features."
  const base::string16 unlock_features =
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_OPT_IN_UNLOCK_MORE_FEATURES);

  // Second substitution string specifies the action to be taken.
  const base::string16 action = GetAction(consent_status);

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

  container_->SetAccessibleName(label_text);

  // After updating the |label_| we need to ensure that it is remeasured and
  // repainted to address a timing bug in which the AssistantOptInView was
  // sometimes drawn in an invalid state (b/130758812).
  container_->Layout();
  container_->SchedulePaint();
}

}  // namespace ash
