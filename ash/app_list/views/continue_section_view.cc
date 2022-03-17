// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/app_list_view_util.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
// Header paddings in dips.
constexpr gfx::Insets kHeaderPadding(0, 12, 4, 12);

// Suggested tasks layout constants.
constexpr size_t kMinFilesForContinueSectionClamshellMode = 3;
constexpr size_t kMinFilesForContinueSectionTabletMode = 2;

// Privacy toast icon size.
constexpr size_t kPrivacyIconSizeClamshell = 60;
constexpr size_t kPrivacyIconSizeTablet = 48;

// Privacy toast interior margin
constexpr gfx::Insets kPrivacyToastInteriorMarginClamshell(12, 12, 12, 16);

// Delay before marking the privacy notice as swhon.
const base::TimeDelta kPrivacyNoticeShownDelay = base::Seconds(6);

// Animation constants.
constexpr base::TimeDelta kDismissToastAnimationDuration =
    base::Milliseconds(200);
constexpr base::TimeDelta kShowSuggestionsAnimationDuration =
    base::Milliseconds(300);

// The vertical inside padding between this view and its parent. Taken from
// AppListBubbleAppsPage.
constexpr int kVerticalPaddingFromParent = 16;

std::unique_ptr<views::Label> CreateContinueLabel(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), bubble_utils::LabelStyle::kSubtitle);
  return label;
}

void CleanupLayer(views::View* view) {
  view->DestroyLayer();
}

}  // namespace

ContinueSectionView::ContinueSectionView(AppListViewDelegate* view_delegate,
                                         int columns,
                                         bool tablet_mode)
    : tablet_mode_(tablet_mode) {
  DCHECK(view_delegate);

  AppListModelProvider::Get()->AddObserver(this);

  if (Shell::HasInstance())
    Shell::Get()->app_list_controller()->AddObserver(this);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                      views::MaximumFlexSizeRule::kUnbounded));

  if (!tablet_mode) {
    continue_label_ = AddChildView(CreateContinueLabel(
        l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_CONTINUE_SECTION_LABEL)));
    continue_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    continue_label_->SetBorder(views::CreateEmptyBorder(kHeaderPadding));
  }

  suggestions_container_ =
      AddChildView(std::make_unique<ContinueTaskContainerView>(
          view_delegate, columns,
          base::BindRepeating(
              &ContinueSectionView::OnSearchResultContainerResultsChanged,
              base::Unretained(this)),
          tablet_mode));

  UpdateElementsVisibility();
}

ContinueSectionView::~ContinueSectionView() {
  AppListModelProvider::Get()->RemoveObserver(this);
  if (Shell::HasInstance() && Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->RemoveObserver(this);
}

size_t ContinueSectionView::GetTasksSuggestionsCount() const {
  return suggestions_container_->num_results();
}

void ContinueSectionView::DisableFocusForShowingActiveFolder(bool disabled) {
  suggestions_container_->DisableFocusForShowingActiveFolder(disabled);

  // Prevent items from being accessed by ChromeVox.
  SetViewIgnoredForAccessibility(this, disabled);
}

ContinueTaskView* ContinueSectionView::GetTaskViewAtForTesting(
    size_t index) const {
  DCHECK_GT(GetTasksSuggestionsCount(), index);
  return static_cast<ContinueTaskView*>(
      suggestions_container_->children()[index]);
}

void ContinueSectionView::UpdateSuggestionTasks() {
  suggestions_container_->SetResults(
      AppListModelProvider::Get()->search_model()->results());
}

void ContinueSectionView::OnSearchResultContainerResultsChanged() {
  MaybeCreatePrivacyNotice();
  MaybeAnimateOutPrivacyNotice();
}

bool ContinueSectionView::HasMinimumFilesToShow() const {
  return suggestions_container_->num_results() >=
         (tablet_mode_ ? kMinFilesForContinueSectionTabletMode
                       : kMinFilesForContinueSectionClamshellMode);
}

bool ContinueSectionView::ShouldShowPrivacyNotice() const {
  if (!nudge_controller_)
    return false;

  // Don't show the privacy notice if the reorder nudge is showing.
  if (nudge_controller_->current_nudge() ==
      AppListNudgeController::NudgeType::kReorderNudge) {
    return false;
  }

  return HasMinimumFilesToShow() &&
         !(nudge_controller_->IsPrivacyNoticeAccepted() ||
           nudge_controller_->WasPrivacyNoticeShown());
}

bool ContinueSectionView::ShouldShowFilesSection() const {
  return HasMinimumFilesToShow() &&
         (nudge_controller_->IsPrivacyNoticeAccepted() ||
          nudge_controller_->WasPrivacyNoticeShown()) &&
         !privacy_toast_;
}

void ContinueSectionView::SetShownInBackground(bool shown_in_background) {
  // If the privacy notice becomes inactive when it is shown in background, stop
  // the privacy notice shown timer to restart the count on the next show.
  if (shown_in_background && privacy_notice_shown_timer_.IsRunning()) {
    privacy_notice_shown_timer_.AbandonAndStop();
    return;
  }

  // If the privacy notice becomes active again, restart the
  // `privacy_notice_shown_timer_`.
  if (!shown_in_background && !nudge_controller_->WasPrivacyNoticeShown() &&
      privacy_toast_) {
    privacy_notice_shown_timer_.Start(
        FROM_HERE, kPrivacyNoticeShownDelay,
        base::BindOnce(&ContinueSectionView::OnPrivacyNoticeShowTimerDone,
                       base::Unretained(this)));
  }
}

void ContinueSectionView::SetNudgeController(
    AppListNudgeController* nudge_controller) {
  nudge_controller_ = nudge_controller;
}

void ContinueSectionView::OnPrivacyToastAcknowledged() {
  if (nudge_controller_) {
    nudge_controller_->SetPrivacyNoticeAcceptedPref(true);
    nudge_controller_->SetPrivacyNoticeShown(false);
  }
  AnimateDismissToast(
      base::BindRepeating(&ContinueSectionView::AnimateShowContinueSection,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ContinueSectionView::AnimateDismissToast(base::RepeatingClosure callback) {
  PrepareForLayerAnimation(privacy_toast_);

  views::AnimationBuilder animation_builder;
  animation_builder.OnEnded(std::move(callback));
  animation_builder.OnAborted(std::move(callback));

  animation_builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetOpacity(privacy_toast_, 0, gfx::Tween::LINEAR)
      .SetDuration(kDismissToastAnimationDuration);
}

void ContinueSectionView::AnimateSlideLauncherContent(int vertical_offset) {
  RemovePrivacyNotice();
  // The sibling views from this should slide down the total of the height
  // difference to make room for the continue section suggestions and title.
  for (views::View* view : parent()->children()) {
    if (view == this)
      continue;
    const bool create_layer = PrepareForLayerAnimation(view);
    auto cleanup = create_layer ? base::BindRepeating(&CleanupLayer, view)
                                : base::DoNothing();
    StartSlideInAnimation(view, vertical_offset, base::Milliseconds(300),
                          gfx::Tween::ACCEL_40_DECEL_100_3, cleanup);
  }
}

void ContinueSectionView::AnimateShowContinueSection() {
  int height_difference = privacy_toast_->GetPreferredSize().height() -
                          suggestions_container_->GetPreferredSize().height();

  const gfx::Tween::Type animation_tween = gfx::Tween::ACCEL_40_DECEL_100_3;

  // The initial position for the launcher continue section should be right
  // below the search divider.
  gfx::Transform initial_transform;
  initial_transform.Translate(0, -kVerticalPaddingFromParent);

  if (continue_label_) {
    height_difference -= continue_label_->GetPreferredSize().height();
    PrepareForLayerAnimation(continue_label_);
    continue_label_->SetVisible(true);
    continue_label_->layer()->SetOpacity(0);
    continue_label_->layer()->SetTransform(initial_transform);
  }

  PrepareForLayerAnimation(suggestions_container_);
  suggestions_container_->layer()->SetTransform(initial_transform);

  privacy_toast_->DestroyLayer();
  privacy_toast_->SetVisible(false);

  suggestions_container_->AnimateSlideInSuggestions(
      height_difference + kVerticalPaddingFromParent,
      kShowSuggestionsAnimationDuration, animation_tween);

  AnimateSlideLauncherContent(height_difference);

  auto cleanup = continue_label_
                     ? base::BindRepeating(&CleanupLayer, continue_label_)
                     : base::DoNothing();

  views::AnimationBuilder animation_builder;
  animation_builder.OnEnded(cleanup);
  animation_builder.OnAborted(cleanup);
  animation_builder.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation_builder.Once();

  animation_builder.GetCurrentSequence().SetTransform(
      suggestions_container_, gfx::Transform(), animation_tween);

  if (continue_label_) {
    animation_builder.GetCurrentSequence().SetOpacity(continue_label_, 1.0,
                                                      animation_tween);
    animation_builder.GetCurrentSequence().SetTransform(
        continue_label_, gfx::Transform(), animation_tween);
  }

  animation_builder.GetCurrentSequence().SetDuration(
      kShowSuggestionsAnimationDuration);
}

void ContinueSectionView::RemovePrivacyNotice() {
  if (privacy_toast_) {
    RemoveChildViewT(privacy_toast_);
    privacy_toast_ = nullptr;
  }
  UpdateElementsVisibility();
}

void ContinueSectionView::OnPrivacyNoticeShowTimerDone() {
  if (!nudge_controller_)
    return;

  nudge_controller_->SetPrivacyNoticeShownPref(true);
}

void ContinueSectionView::MaybeCreatePrivacyNotice() {
  DCHECK(nudge_controller_);

  if (!ShouldShowPrivacyNotice()) {
    // Reset the nudge controller state if privacy notice shouldn't be showing.
    if (nudge_controller_->current_nudge() ==
        AppListNudgeController::NudgeType::kPrivacyNotice) {
      nudge_controller_->SetPrivacyNoticeShown(false);
    }

    return;
  }

  nudge_controller_->SetPrivacyNoticeShown(true);

  if (privacy_toast_)
    return;

  // When creating the privacy notice and after a delay, mark the nudge as
  // shown, which will keep the user from seeing the nudge again.
  privacy_notice_shown_timer_.Start(
      FROM_HERE, kPrivacyNoticeShownDelay,
      base::BindOnce(&ContinueSectionView::OnPrivacyNoticeShowTimerDone,
                     base::Unretained(this)));

  const int privacy_toast_text_id =
      tablet_mode_ ? IDS_ASH_LAUNCHER_CONTINUE_SECTION_PRIVACY_TEXT_TABLET
                   : IDS_ASH_LAUNCHER_CONTINUE_SECTION_PRIVACY_TEXT;

  privacy_toast_ = AddChildView(
      AppListToastView::Builder(
          l10n_util::GetStringUTF16(privacy_toast_text_id))
          .SetButton(l10n_util::GetStringUTF16(
                         IDS_ASH_LAUNCHER_CONTINUE_SECTION_PRIVACY_BUTTON),
                     base::BindRepeating(
                         &ContinueSectionView::OnPrivacyToastAcknowledged,
                         base::Unretained(this)))
          .SetStyleForTabletMode(tablet_mode_)
          .SetThemingIcons(&kContinueFilesDarkIcon, &kContinueFilesLightIcon)
          .SetIconSize(tablet_mode_ ? kPrivacyIconSizeTablet
                                    : kPrivacyIconSizeClamshell)
          .SetIconBackground(true)
          .Build());
  privacy_toast_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  if (!tablet_mode_)
    privacy_toast_->UpdateInteriorMargins(kPrivacyToastInteriorMarginClamshell);
}

bool ContinueSectionView::FirePrivacyNoticeShownTimerForTest() {
  if (!privacy_notice_shown_timer_.IsRunning())
    return false;

  privacy_notice_shown_timer_.FireNow();
  return true;
}

void ContinueSectionView::MaybeAnimateOutPrivacyNotice() {
  if (Shell::HasInstance() && Shell::Get()->app_list_controller() &&
      Shell::Get()->app_list_controller()->IsVisible() && !tablet_mode_ &&
      privacy_toast_ && !ShouldShowPrivacyNotice()) {
    AnimateDismissToast(
        base::BindRepeating(&ContinueSectionView::AnimateSlideLauncherContent,
                            weak_ptr_factory_.GetWeakPtr(),
                            privacy_toast_->GetPreferredSize().height()));
    return;
  }
  UpdateElementsVisibility();
}

void ContinueSectionView::UpdateElementsVisibility() {
  const bool show_files_section = ShouldShowFilesSection();
  const bool show_privacy_notice = ShouldShowPrivacyNotice();
  SetVisible(show_files_section || show_privacy_notice);
  suggestions_container_->SetVisible(show_files_section);
  if (continue_label_)
    continue_label_->SetVisible(show_files_section);
}

void ContinueSectionView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void ContinueSectionView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void ContinueSectionView::OnDidChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  // Tablet mode does not have a scrollable container or continue label.
  if (tablet_mode_)
    return;
  // Nothing to do if views are losing focus.
  if (!focused_now)
    return;
  // If a child of the privacy toast gained focus (e.g. the OK button) then
  // ensure the whole toast is visible.
  if (privacy_toast_ && privacy_toast_->Contains(focused_now)) {
    privacy_toast_->ScrollViewToVisible();
    return;
  }
  // If a suggested task gained focus then ensure the continue label is visible
  // so the user knows what this section is.
  if (suggestions_container_->Contains(focused_now)) {
    DCHECK(continue_label_);
    continue_label_->ScrollViewToVisible();
  }
}

void ContinueSectionView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  UpdateSuggestionTasks();
}

void ContinueSectionView::OnAppListVisibilityChanged(bool shown,
                                                     int64_t display_id) {
  if (!shown && privacy_toast_) {
    RemoveChildViewT(privacy_toast_);
    privacy_toast_ = nullptr;
  }

  // Update the nudge type in nudge controller if the privacy notice is
  // considered shown and will not be shown again.
  if (!shown && nudge_controller_->WasPrivacyNoticeShown() &&
      nudge_controller_->current_nudge() ==
          AppListNudgeController::NudgeType::kPrivacyNotice) {
    nudge_controller_->SetPrivacyNoticeShown(false);
  }

  // When hiding the launcher, stop the privacy notice shown timer to restart
  // the count on the next show.
  if (!shown && privacy_notice_shown_timer_.IsRunning())
    privacy_notice_shown_timer_.AbandonAndStop();

  if (shown)
    MaybeCreatePrivacyNotice();
  UpdateElementsVisibility();
}

BEGIN_METADATA(ContinueSectionView, views::View)
END_METADATA

}  // namespace ash
