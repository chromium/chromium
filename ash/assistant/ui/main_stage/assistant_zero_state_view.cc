// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/main_stage/assistant_zero_state_view.h"

#include <memory>

#include "ash/assistant/model/assistant_ui_model.h"
#include "ash/assistant/ui/assistant_ui_constants.h"
#include "ash/assistant/ui/assistant_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/assistant/ui/main_stage/assistant_onboarding_view.h"
#include "ash/assistant/ui/main_stage/launcher_search_iph_view.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "chromeos/ash/services/assistant/public/cpp/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
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

constexpr auto kIphMarginDip = gfx::Insets::TLBR(0, 24, 2, 24);
constexpr auto kIphMarginTabletModeDip = gfx::Insets::TLBR(12, 16, 2, 16);

bool ShouldShowGreetingOrOnboarding(bool in_tablet_mode) {
  if (assistant::features::IsAssistantLearnMoreEnabled()) {
    return !in_tablet_mode;
  }
  return true;
}

bool ShouldShowSpacer(bool in_tablet_mode) {
  if (assistant::features::IsAssistantLearnMoreEnabled()) {
    return !in_tablet_mode;
  }
  return false;
}

bool ShouldShowIph() {
  return assistant::features::IsAssistantLearnMoreEnabled();
}

int GetMarginWidth(bool in_tablet_mode) {
  return in_tablet_mode ? kIphMarginTabletModeDip.width()
                        : kIphMarginDip.width();
}

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

void AssistantZeroStateView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  if (prev_bounds.size() != bounds().size()) {
    // Update `learn_more_toast_` preferred size to layout the title label.
    // The actual height may change based on the text in the toast.
    const int kIphMaxHeightDip = 64;
    const auto kIphPreferredSizeDip =
        gfx::Size(bounds().width() - GetMarginWidth(delegate_->IsTabletMode()),
                  kIphMaxHeightDip);
    iph_view_->SetPreferredSize(kIphPreferredSizeDip);
  }
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
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

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
  greeting_label_->SetBackgroundColorId(kColorAshAssistantBgPlate);
  greeting_label_->SetEnabledColorId(kColorAshAssistantTextColorPrimary);

  // Spacer.
  spacer_ = AddChildView(std::make_unique<views::View>());
  layout->SetFlexForView(spacer_, 1);

  // Launcher search IPH view:
  iph_view_ = AddChildView(std::make_unique<LauncherSearchIphView>(
      /*delegate=*/this, /*is_in_tablet_mode=*/delegate_->IsTabletMode()));
  iph_view_->SetID(AssistantViewID::kLauncherSearchIph);
  iph_view_->SetProperty(views::kMarginsKey, delegate_->IsTabletMode()
                                                 ? kIphMarginTabletModeDip
                                                 : kIphMarginDip);
}

void AssistantZeroStateView::UpdateLayout() {
  const bool show_greeting_or_onboarding =
      ShouldShowGreetingOrOnboarding(delegate_->IsTabletMode());
  const bool show_onboarding = delegate_->ShouldShowOnboarding();
  onboarding_view_->SetVisible(show_greeting_or_onboarding && show_onboarding);
  greeting_label_->SetVisible(show_greeting_or_onboarding && !show_onboarding);

  const bool show_spacer = ShouldShowSpacer(delegate_->IsTabletMode());
  spacer_->SetVisible(show_spacer);

  const bool show_iph = ShouldShowIph();
  iph_view_->SetVisible(show_iph);
}

void AssistantZeroStateView::RunLauncherSearchQuery(
    const std::u16string& query) {
  delegate_->OnLauncherSearchChipPressed(query);
}

void AssistantZeroStateView::OpenAssistantPage() {
  NOTREACHED_NORETURN();
}

}  // namespace ash
