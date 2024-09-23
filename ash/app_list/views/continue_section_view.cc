// Copyright 2021 The Chromium Authors
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
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/desks/templates/saved_desk_controller.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {
// Whether the files section has been shown.
bool g_continue_section_files_shown = false;

// Suggested tasks layout constants.
constexpr size_t kMinFilesForContinueSectionClamshellMode = 3;
constexpr size_t kMinFilesForContinueSectionTabletMode = 2;

// Privacy toast icon size.
constexpr size_t kPrivacyIconSizeClamshell = 60;
constexpr size_t kPrivacyIconSizeTablet = 48;

// Privacy toast interior margin
constexpr auto kPrivacyToastInteriorMarginClamshell =
    gfx::Insets::TLBR(12, 12, 12, 16);

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

void CleanupLayer(views::View* view) {
  view->DestroyLayer();
}

}  // namespace

ContinueSectionView::ContinueSectionView(AppListViewDelegate* view_delegate,
                                         int columns,
                                         bool tablet_mode)
    : view_delegate_(view_delegate), tablet_mode_(tablet_mode) {
  DCHECK(view_delegate_);

  AppListModelProvider::Get()->AddObserver(this);

  if (Shell::HasInstance())
    Shell::Get()->app_list_controller()->AddObserver(this);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetOrientation(views::LayoutOrientation::kVertical)
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::LayoutOrientation::kHorizontal,
                      views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                      views::MaximumFlexSizeRule::kUnbounded));

  suggestions_container_ =
      AddChildView(std::make_unique<ContinueTaskContainerView>(
          view_delegate, columns,
          base::BindRepeating(
              &ContinueSectionView::OnSearchResultContainerResultsChanged,
              base::Unretained(this)),
          tablet_mode));
  suggestions_container_->SetVisible(false);
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
  if (privacy_toast_)
    privacy_toast_->toast_button()->SetEnabled(!disabled);

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

// static
bool ContinueSectionView::EnableContinueSectionFileRemovalMetrics() {
  return g_continue_section_files_shown;
}

// static
void ContinueSectionView::
    ResetContinueSectionFileRemovalMetricEnabledForTest() {
  g_continue_section_files_shown = false;
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
  return suggestions_container_->num_file_results() >=
         (tablet_mode_ ? kMinFilesForContinueSectionTabletMode
                       : kMinFilesForContinueSectionClamshellMode);
}

bool ContinueSectionView::HasDesksAdminTemplates() const {
  return suggestions_container_->num_desks_admin_template_results() > 0;
}

bool ContinueSectionView::ShouldShowPrivacyNotice() const {
  if (!nudge_controller_)
    return false;

  // Don't show the privacy notice if the reorder nudge is showing.
  if (nudge_controller_->current_nudge() ==
      AppListNudgeController::NudgeType::kReorderNudge) {
    return false;
  }

  return (HasDesksAdminTemplates() || HasMinimumFilesToShow()) &&
         !(nudge_controller_->IsPrivacyNoticeAccepted() ||
           nudge_controller_->WasPrivacyNoticeShown());
}

bool ContinueSectionView::ShouldShowFilesSection() const {
  return (HasDesksAdminTemplates() || HasMinimumFilesToShow()) &&
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
  // Prevents setting up new animation if the toast is already hiding.
  // https://crbug.com/1326237.
  DCHECK(privacy_toast_);
  if (privacy_toast_->layer() &&
      privacy_toast_->layer()->GetTargetOpacity() == 0.f) {
    return;
  }

  PrepareForLayerAnimation(privacy_toast_);

  views::AnimationBuilder animation_builder;
  animation_builder.OnEnded(callback);
  animation_builder.OnAborted(callback);

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

  PrepareForLayerAnimation(suggestions_container_);
  suggestions_container_->layer()->SetTransform(initial_transform);

  privacy_toast_->DestroyLayer();
  privacy_toast_->SetVisible(false);

  suggestions_container_->AnimateSlideInSuggestions(
      height_difference + kVerticalPaddingFromParent,
      kShowSuggestionsAnimationDuration, animation_tween);

  AnimateSlideLauncherContent(height_difference);

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetTransform(suggestions_container_, gfx::Transform(), animation_tween)
      .SetDuration(kShowSuggestionsAnimationDuration);
}

void ContinueSectionView::RemovePrivacyNotice() {
  if (privacy_toast_) {
    RemoveChildViewT(privacy_toast_.get());
    privacy_toast_ = nullptr;
  }
  UpdateElementsVisibility();
  PreferredSizeChanged();
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
          .SetIcon(
              ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
                  IDR_APP_LIST_CONTINUE_SECTION_NOTICE_IMAGE))
          .SetIconSize(tablet_mode_ ? kPrivacyIconSizeTablet
                                    : kPrivacyIconSizeClamshell)
          .SetIconBackground(true)
          .Build());
  privacy_toast_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  if (available_width_) {
    privacy_toast_->SetAvailableWidth(*available_width_);
  }

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
  // If the user chose to hide the section, set visibility false and reset the
  // privacy toast.
  if (view_delegate_->ShouldHideContinueSection()) {
    SetVisible(false);
    if (privacy_toast_) {
      RemoveChildViewT(privacy_toast_.get());
      privacy_toast_ = nullptr;
      nudge_controller_->SetPrivacyNoticeShown(false);
      privacy_notice_shown_timer_.AbandonAndStop();
    }
    return;
  }
  const bool show_files_section = ShouldShowFilesSection();
  const bool show_privacy_notice = ShouldShowPrivacyNotice();

  SetVisible(show_files_section || show_privacy_notice);

  if (show_files_section)
    g_continue_section_files_shown = true;

  const bool suggestions_visibility_changed =
      show_files_section != suggestions_container_->GetVisible();
  suggestions_container_->SetVisible(show_files_section);

  if (suggestions_visibility_changed)
    PreferredSizeChanged();
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
    // The parent view owns the continue label, which provides more context
    // for the privacy notice. Ensure the label is visible.
    parent()->ScrollViewToVisible();
    return;
  }
  // If a suggested task gained focus then ensure the continue label is visible
  // so the user knows what this section is.
  if (suggestions_container_->Contains(focused_now)) {
    // The parent view owns the continue label, so ensure label visibility.
    parent()->ScrollViewToVisible();
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
    // Abort any in-progress privacy toast animation. This may delete the toast
    // view. Explicitly aborting the animation avoids a double-delete in
    // RemoveChildViewT() below. https://crbug.com/1357434
    if (privacy_toast_->layer())
      privacy_toast_->layer()->GetAnimator()->AbortAllAnimations();

    if (privacy_toast_) {
      RemoveChildViewT(privacy_toast_.get());
      privacy_toast_ = nullptr;
    }
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

  if (privacy_toast_)
    PreferredSizeChanged();
}

void ContinueSectionView::ConfigureLayoutForAvailableWidth(
    int available_width) {
  available_width_ = available_width;

  if (privacy_toast_) {
    privacy_toast_->SetAvailableWidth(available_width);
  }
}

BEGIN_METADATA(ContinueSectionView)
END_METADATA

}  // namespace ash
