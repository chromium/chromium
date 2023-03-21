// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"

#include <memory>

#include "ash/app_list/views/app_list_toast_view.h"  //nogncheck
#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/colors/assistant_colors.h"
#include "ash/assistant/ui/colors/assistant_colors_util.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/strings/string_piece.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_provider.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

namespace ash {

namespace {

// Appearance.
constexpr int kGreetingLabelTopMarginDip = 28;
constexpr int kOnboardingViewTopMarginDip = 48;

constexpr base::StringPiece kLearnMoreUrl =
    "https://support.google.com/chromebook?p=assistant";
constexpr auto kToastMarginDip = gfx::Insets::TLBR(0, 24, 4, 24);
constexpr auto kToastPreferredSizeDip = gfx::Size(496, 64);

}  // namespace

AssistantZeroStateView::AssistantZeroStateView(AssistantViewDelegate* delegate)
    : delegate_(delegate) {
  SetID(AssistantViewID::kZeroStateView);

  InitLayout();
  UpdateLayout();

  assistant_controller_observation_.Observe(AssistantController::Get());
  AssistantUiController::Get()->GetModel()->AddObserver(this);
}

AssistantZeroStateView::~AssistantZeroStateView() {
  if (AssistantUiController::Get())
    AssistantUiController::Get()->GetModel()->RemoveObserver(this);
}

const char* AssistantZeroStateView::GetClassName() const {
  return "AssistantZeroStateView";
}

gfx::Size AssistantZeroStateView::CalculatePreferredSize() const {
  return gfx::Size(INT_MAX, GetHeightForWidth(INT_MAX));
}

void AssistantZeroStateView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantZeroStateView::OnThemeChanged() {
  views::View::OnThemeChanged();

  greeting_label_->SetBackgroundColor(ash::assistant::ResolveAssistantColor(
      assistant_colors::ColorName::kBgAssistantPlate));

  greeting_label_->SetEnabledColor(
      GetColorProvider()->GetColor(kColorAshAssistantTextColorPrimary));
}

void AssistantZeroStateView::OnAssistantControllerDestroying() {
  AssistantUiController::Get()->GetModel()->RemoveObserver(this);
  DCHECK(assistant_controller_observation_.IsObservingSource(
      AssistantController::Get()));
  assistant_controller_observation_.Reset();
}

void AssistantZeroStateView::OnUiVisibilityChanged(
    AssistantVisibility new_visibility,
    AssistantVisibility old_visibility,
    absl::optional<AssistantEntryPoint> entry_point,
    absl::optional<AssistantExitPoint> exit_point) {
  if (new_visibility == AssistantVisibility::kClosed)
    UpdateLayout();
}

void AssistantZeroStateView::InitLayout() {
  // Layout.
  views::BoxLayout* layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical));

  // Onboarding.
  onboarding_view_ =
      AddChildView(std::make_unique<AssistantOnboardingView>(delegate_));
  onboarding_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kOnboardingViewTopMarginDip, 0, 0, 0)));

  // Greeting.
  greeting_label_ = AddChildView(std::make_unique<views::Label>());
  greeting_label_->SetID(AssistantViewID::kGreetingLabel);
  greeting_label_->SetAutoColorReadabilityEnabled(false);
  greeting_label_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kGreetingLabelTopMarginDip, 0, 0, 0)));
  greeting_label_->SetFontList(
      assistant::ui::GetDefaultFontList()
          .DeriveWithSizeDelta(8)
          .DeriveWithWeight(gfx::Font::Weight::MEDIUM));
  greeting_label_->SetHorizontalAlignment(
      gfx::HorizontalAlignment::ALIGN_CENTER);
  greeting_label_->SetMultiLine(true);
  greeting_label_->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_ASSISTANT_PROMPT_DEFAULT));

  if (assistant::features::IsAssistantLearnMoreEnabled()) {
    // Spacer.
    auto* spacer = AddChildView(std::make_unique<views::View>());
    layout->SetFlexForView(spacer, 1);

    // Learn more toast.
    // TODO(b/274527683, b/274525194): add i18n and a11y supports.
    learn_more_toast_ = AddChildView(
        AppListToastView::Builder(u"Learn more about Google Assistant")
            .SetButton(l10n_util::GetStringUTF16(IDS_ASH_LEARN_MORE),
                       base::BindRepeating(
                           &AssistantZeroStateView::OnLearnMoreButtonPressed,
                           base::Unretained(this)))
            .Build());
    learn_more_toast_->SetID(AssistantViewID::kLearnMoreToast);
    learn_more_toast_->SetProperty(views::kMarginsKey, kToastMarginDip);
    learn_more_toast_->SetPreferredSize(kToastPreferredSizeDip);
    learn_more_toast_->SetTitleLabelMaximumWidth();
  }
}

void AssistantZeroStateView::UpdateLayout() {
  const bool show_onboarding = delegate_->ShouldShowOnboarding();
  onboarding_view_->SetVisible(show_onboarding);
  greeting_label_->SetVisible(!show_onboarding);
}

void AssistantZeroStateView::OnLearnMoreButtonPressed() {
  AssistantController::Get()->OpenUrl(GURL(kLearnMoreUrl));
}

}  // namespace ash
