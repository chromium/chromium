// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/app_list_nudge_controller.h"
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A flag to enable/disable the privacy toast for test.
bool g_nudge_accepted_for_test = false;

// Header paddings in dips.
constexpr int kHeaderHorizontalPadding = 12;

// Suggested tasks layout constants.
constexpr size_t kMinFilesForContinueSectionClamshellMode = 3;
constexpr size_t kMinFilesForContinueSectionTabletMode = 2;

// Privacy toast icon size.
constexpr size_t kPrivacyIconSizeClamshell = 60;
constexpr size_t kPrivacyIconSizeTablet = 48;

// Privacy toast interior margin
constexpr gfx::Insets kPrivacyToastInteriorMargin(12, 12, 12, 16);

// Delay before marking the privacy notice as swhon.
const base::TimeDelta kPrivacyNoticeShownDelay = base::Seconds(6);

// Privacy notice dictionary pref keys.
const char kPrivacyNoticeAcceptedKey[] = "accepted";
const char kPrivacyNoticeShownKey[] = "shown";

std::unique_ptr<views::Label> CreateContinueLabel(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), bubble_utils::LabelStyle::kSubtitle);
  return label;
}

PrefService* GetActiveUserPrefService() {
  if (!Shell::HasInstance())
    return nullptr;

  DCHECK(Shell::Get()->session_controller()->IsActiveUserSessionStarted());

  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(pref_service);
  return pref_service;
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
    continue_label_->SetBorder(
        views::CreateEmptyBorder(gfx::Insets(0, kHeaderHorizontalPadding)));
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

void ContinueSectionView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  UpdateSuggestionTasks();
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
  UpdateElementsVisibility();
}

bool ContinueSectionView::HasMinimumFilesToShow() const {
  return suggestions_container_->num_results() >=
         (tablet_mode_ ? kMinFilesForContinueSectionTabletMode
                       : kMinFilesForContinueSectionClamshellMode);
}

bool ContinueSectionView::ShouldShowPrivacyNotice() const {
  // Don't show the privacy notice if the reorder nudge is showing.
  if (nudge_controller_ &&
      nudge_controller_->current_nudge() ==
          AppListNudgeController::NudgeType::kReorderNudge) {
    return false;
  }

  return HasMinimumFilesToShow() &&
         !(IsPrivacyNoticeAccepted() || IsPrivacyNoticeShown());
}

bool ContinueSectionView::ShouldShowFilesSection() const {
  return HasMinimumFilesToShow() && !ShouldShowPrivacyNotice() &&
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
  if (!shown_in_background && !IsPrivacyNoticeShown() && privacy_toast_) {
    privacy_notice_shown_timer_.Start(
        FROM_HERE, kPrivacyNoticeShownDelay,
        base::BindOnce(&ContinueSectionView::MarkPrivacyNoticeShown,
                       base::Unretained(this)));
  }
}

void ContinueSectionView::SetNudgeController(
    AppListNudgeController* nudge_controller) {
  nudge_controller_ = nudge_controller;
}

void ContinueSectionView::MarkPrivacyNoticeAccepted() {
  {
    DictionaryPrefUpdate privacy_pref_update(
        GetActiveUserPrefService(), prefs::kLauncherFilesPrivacyNotice);
    privacy_pref_update->SetBoolKey(kPrivacyNoticeAcceptedKey, true);
  }
  if (privacy_toast_) {
    RemoveChildViewT(privacy_toast_);
    privacy_toast_ = nullptr;
  }
  nudge_controller_->SetPrivacyNoticeShown(false);
  UpdateElementsVisibility();
}

void ContinueSectionView::MarkPrivacyNoticeShown() {
  DictionaryPrefUpdate privacy_pref_update(GetActiveUserPrefService(),
                                           prefs::kLauncherFilesPrivacyNotice);
  privacy_pref_update->SetBoolKey(kPrivacyNoticeShownKey, true);
}

void ContinueSectionView::MaybeCreatePrivacyNotice() {
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
      base::BindOnce(&ContinueSectionView::MarkPrivacyNoticeShown,
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
                         &ContinueSectionView::MarkPrivacyNoticeAccepted,
                         base::Unretained(this)))
          .SetStyleForTabletMode(tablet_mode_)
          .SetThemingIcons(&kContinueFilesDarkIcon, &kContinueFilesLightIcon)
          .SetIconSize(tablet_mode_ ? kPrivacyIconSizeTablet
                                    : kPrivacyIconSizeClamshell)
          .Build());
  privacy_toast_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kScaleToMaximum));
  privacy_toast_->UpdateInteriorMargins(kPrivacyToastInteriorMargin);
}

bool ContinueSectionView::FirePrivacyNoticeShownTimerForTest() {
  if (!privacy_notice_shown_timer_.IsRunning())
    return false;

  privacy_notice_shown_timer_.FireNow();
  return true;
}

void ContinueSectionView::UpdateElementsVisibility() {
  const bool show_files_section = ShouldShowFilesSection();
  const bool show_privacy_notice = ShouldShowPrivacyNotice();
  SetVisible(show_files_section || show_privacy_notice);
  suggestions_container_->SetVisible(show_files_section);
  if (continue_label_)
    continue_label_->SetVisible(show_files_section);
}

bool ContinueSectionView::IsPrivacyNoticeAccepted() const {
  if (g_nudge_accepted_for_test)
    return true;

  const PrefService* prefs = GetActiveUserPrefService();
  if (!prefs)
    return false;

  const base::Value* result = prefs->Get(prefs::kLauncherFilesPrivacyNotice)
                                  ->FindKey(kPrivacyNoticeAcceptedKey);
  if (!result || !result->is_bool())
    return false;

  return result->GetBool();
}

bool ContinueSectionView::IsPrivacyNoticeShown() const {
  const PrefService* prefs = GetActiveUserPrefService();
  if (!prefs)
    return false;

  const base::Value* result = prefs->Get(prefs::kLauncherFilesPrivacyNotice)
                                  ->FindKey(kPrivacyNoticeShownKey);
  if (!result || !result->is_bool())
    return false;

  return result->GetBool();
}

void ContinueSectionView::OnAppListVisibilityChanged(bool shown,
                                                     int64_t display_id) {
  if (!shown && privacy_toast_) {
    RemoveChildViewT(privacy_toast_);
    privacy_toast_ = nullptr;
  }

  // Update the nudge type in nudge controller if the privacy notice is
  // considered shown and will not be shown again.
  if (!shown && IsPrivacyNoticeShown() &&
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

// static
void ContinueSectionView::SetPrivacyNoticeAcceptedForTest(bool is_disabled) {
  g_nudge_accepted_for_test = is_disabled;
}

BEGIN_METADATA(ContinueSectionView, views::View)
END_METADATA

}  // namespace ash
