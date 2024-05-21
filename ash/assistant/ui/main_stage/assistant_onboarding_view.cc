// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/assistant/model/assistant_suggestions_model.h"
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_suggestion_view.h"
#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/table_layout_view.h"

namespace ash {

namespace {

using assistant::AssistantSuggestion;
using assistant::AssistantSuggestionType;

// Greeting.
constexpr int kGreetingLabelLineHeight = 28;
constexpr int kGreetingLabelSizeDelta = 10;

// Intro.
constexpr int kIntroLabelLineHeight = 20;
constexpr int kIntroLabelMarginTopDip = 12;
constexpr int kIntroLabelSizeDelta = 2;

// Suggestions.
constexpr int kSuggestionsColumnCount = 3;
constexpr int kSuggestionsMaxCount = 6;
constexpr int kSuggestionsMarginDip = 16;
constexpr int kSuggestionsMarginTopDip = 32;

// Helpers ---------------------------------------------------------------------

std::string GetGreetingMessage(AssistantViewDelegate* delegate) {
  base::Time::Exploded now;
  base::Time::Now().LocalExplode(&now);

  const std::string given_name = delegate->GetPrimaryUserGivenName();

  if (now.hour < 5) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT,
                     base::UTF8ToUTF16(given_name));
  }

  if (now.hour < 12) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_MORNING_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_MORNING,
                     base::UTF8ToUTF16(given_name));
  }

  if (now.hour < 17) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_AFTERNOON_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_AFTERNOON,
                     base::UTF8ToUTF16(given_name));
  }

  if (now.hour < 23) {
    return given_name.empty()
               ? l10n_util::GetStringUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_EVENING_WITHOUT_NAME)
               : l10n_util::GetStringFUTF8(
                     IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_EVENING,
                     base::UTF8ToUTF16(given_name));
  }

  return given_name.empty()
             ? l10n_util::GetStringUTF8(
                   IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT_WITHOUT_NAME)
             : l10n_util::GetStringFUTF8(
                   IDS_ASSISTANT_BETTER_ONBOARDING_GREETING_NIGHT,
                   base::UTF8ToUTF16(given_name));
}

}  // namespace

// AssistantOnboardingView -----------------------------------------------------

AssistantOnboardingView::AssistantOnboardingView(
    AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kOnboardingView);
  InitLayout();

  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantSuggestionsController::Get()->GetModel()->AddObserver(this);
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantOnboardingView::~AssistantOnboardingView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);

  if (AssistantSuggestionsController::Get())
    AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
}

gfx::Size AssistantOnboardingView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      INT_MAX, GetLayoutManager()->GetPreferredHeightForWidth(this, INT_MAX));
}

void AssistantOnboardingView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantOnboardingView::OnThemeChanged() {
  views::View::OnThemeChanged();

  SkColor greeting_color =
      GetColorProvider()->GetColor(kColorAshAssistantGreetingEnabled);
  greeting_->SetEnabledColor(greeting_color);
  intro_->SetEnabledColor(greeting_color);
}

void AssistantOnboardingView::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  AssistantSuggestionsController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void AssistantOnboardingView::OnOnboardingSuggestionsChanged(
    const std::vector<AssistantSuggestion>& onboarding_suggestions) {
  UpdateSuggestions();
}

void AssistantOnboardingView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
  if (new_visibility != AssistantVisibility::kVisible)
    return;

  UpdateGreeting();

  if (IsDrawn())
    delegate_->OnOnboardingShown();
}

void AssistantOnboardingView::InitLayout() {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical,
      gfx::Insets::VH(0, assistant::ui::kHorizontalMargin)));

  // Greeting.
  greeting_ = AddChildView(std::make_unique<views::Label>());
  greeting_->SetAutoColorReadabilityEnabled(false);
  greeting_->SetFontList(assistant::ui::GetDefaultFontList()
                             .DeriveWithSizeDelta(kGreetingLabelSizeDelta)
                             .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  greeting_->SetLineHeight(kGreetingLabelLineHeight);
  greeting_->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));

  // Intro.
  intro_ = AddChildView(std::make_unique<views::Label>());
  intro_->SetAutoColorReadabilityEnabled(false);
  intro_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kIntroLabelMarginTopDip, 0, 0, 0)));
  intro_->SetFontList(assistant::ui::GetDefaultFontList()
                          .DeriveWithSizeDelta(kIntroLabelSizeDelta)
                          .DeriveWithWeight(gfx::Font ::Weight::MEDIUM));
  intro_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
  intro_->SetLineHeight(kIntroLabelLineHeight);
  intro_->SetMultiLine(true);
  intro_->SetText(
      l10n_util::GetStringUTF16(IDS_ASSISTANT_BETTER_ONBOARDING_INTRO));

  // Suggestions.
  UpdateSuggestions();
}

void AssistantOnboardingView::UpdateSuggestions() {
  if (table_)
    RemoveChildViewT(table_.get());

  table_ = AddChildView(std::make_unique<views::TableLayoutView>());
  table_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kSuggestionsMarginTopDip, 0, 0, 0)));

  // Initialize columns.
  for (int i = 0; i < kSuggestionsColumnCount; ++i) {
    if (i > 0) {
      table_->AddPaddingColumn(
          /*horizontal_resize=*/views::TableLayout::kFixedSize,
          /*width=*/kSuggestionsMarginDip);
    }
    table_->AddColumn(
        /*h_align=*/views::LayoutAlignment::kStretch,
        /*v_align=*/views::LayoutAlignment::kStretch, /*horizontal_resize=*/1.0,
        /*size_type=*/views::TableLayout::ColumnSize::kFixed,
        /*fixed_width=*/0, /*min_width=*/0);
  }

  const std::vector<AssistantSuggestion>& suggestions =
      AssistantSuggestionsController::Get()
          ->GetModel()
          ->GetOnboardingSuggestions();

  // Initialize suggestions.
  for (size_t i = 0; i < suggestions.size() && i < kSuggestionsMaxCount; ++i) {
    if (i % kSuggestionsColumnCount == 0) {
      if (i > 0) {
        table_->AddPaddingRow(
            /*vertical_resize=*/views::TableLayout::kFixedSize,
            /*height=*/kSuggestionsMarginDip);
      }
      table_->AddRows(/*n=*/1,
                      /*vertical_resize=*/views::TableLayout::kFixedSize);
    }
    table_->AddChildView(std::make_unique<AssistantOnboardingSuggestionView>(
        delegate_, suggestions.at(i), i));
  }
}

void AssistantOnboardingView::UpdateGreeting() {
  greeting_->SetText(base::UTF8ToUTF16(GetGreetingMessage(delegate_)));
}

BEGIN_METADATA(AssistantOnboardingView)
END_METADATA

}  // namespace ash
