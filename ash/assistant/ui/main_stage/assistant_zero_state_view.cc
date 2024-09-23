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
#include "ash/public/cpp/assistant/assistant_state.h"
#include "ash/public/cpp/assistant/controller/assistant_controller.h"
#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
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

bool ShouldShowGreetingOrOnboarding(bool in_tablet_mode) {
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHLauncherSearchHelpUiFeature)) {
    return !in_tablet_mode;
  }
  return true;
}

bool ShouldShowIph() {
  return base::FeatureList::IsEnabled(
      feature_engagement::kIPHLauncherSearchHelpUiFeature);
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

gfx::Size AssistantZeroStateView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(
      INT_MAX, GetLayoutManager()->GetPreferredHeightForWidth(this, INT_MAX));
}

void AssistantZeroStateView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void AssistantZeroStateView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  if (prev_bounds.size() != bounds().size()) {
    int height = iph_view_->GetPreferredSize().height();
    auto preferred_size = gfx::Size(bounds().width(), height);
    iph_view_->SetPreferredSize(preferred_size);
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
    std::optional<AssistantEntryPoint> entry_point,
    std::optional<AssistantExitPoint> exit_point) {
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
      /*delegate=*/this, delegate_->IsTabletMode(),
      /*scoped_iph_session=*/nullptr,
      /*location=*/LauncherSearchIphView::UiLocation::kAssistantPage));
  iph_view_->SetID(AssistantViewID::kLauncherSearchIph);
}

void AssistantZeroStateView::UpdateLayout() {
  const bool show_greeting_or_onboarding =
      ShouldShowGreetingOrOnboarding(delegate_->IsTabletMode());
  const bool show_onboarding = delegate_->ShouldShowOnboarding();
  onboarding_view_->SetVisible(show_greeting_or_onboarding && show_onboarding);
  greeting_label_->SetVisible(show_greeting_or_onboarding && !show_onboarding);

  const bool show_iph = ShouldShowIph();
  spacer_->SetVisible(show_iph);
  iph_view_->SetVisible(show_iph);
}

void AssistantZeroStateView::RunLauncherSearchQuery(
    const std::u16string& query) {
  delegate_->OnLauncherSearchChipPressed(query);
}

void AssistantZeroStateView::OpenAssistantPage() {
  NOTREACHED();
}

BEGIN_METADATA(AssistantZeroStateView)
END_METADATA

}  // namespace ash
