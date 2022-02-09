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
#include "ash/app_list/views/app_list_toast_view.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_list/app_list_config.h"
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
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// A flag to enable/disable the privacy toast for test.
bool g_nudge_accepted_for_test = false;

// Header paddings in dips.
constexpr int kHeaderVerticalSpacing = 4;
constexpr int kHeaderHorizontalPadding = 12;

// Suggested tasks layout constants.
constexpr size_t kMinFilesForContinueSectionClamshellMode = 3;
constexpr size_t kMinFilesForContinueSectionTabletMode = 2;

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

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHeaderVerticalSpacing));
  layout->set_main_axis_alignment(
      tablet_mode ? views::BoxLayout::MainAxisAlignment::kCenter
                  : views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

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
  // TODO(crbug.com/1277666): Retrieve other nudges show status to handle edge
  // cases where no two nudges should be shown at the same time in the launcher.
  return HasMinimumFilesToShow() && !(Accepted() || Shown());
}

bool ContinueSectionView::ShouldShowFilesSection() const {
  return HasMinimumFilesToShow() && !ShouldShowPrivacyNotice();
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
  UpdateElementsVisibility();
}

void ContinueSectionView::MarkPrivacyNoticeShown() {
  DictionaryPrefUpdate privacy_pref_update(GetActiveUserPrefService(),
                                           prefs::kLauncherFilesPrivacyNotice);
  privacy_pref_update->SetBoolKey(kPrivacyNoticeShownKey, true);
}

void ContinueSectionView::MaybeCreatePrivacyNotice() {
  if (!ShouldShowPrivacyNotice())
    return;

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
          .Build());
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

bool ContinueSectionView::Accepted() const {
  if (g_nudge_accepted_for_test)
    return true;

  const base::Value* result = GetActiveUserPrefService()
                                  ->Get(prefs::kLauncherFilesPrivacyNotice)
                                  ->FindKey(kPrivacyNoticeAcceptedKey);
  if (!result || !result->is_bool())
    return false;

  return result->GetBool();
}

bool ContinueSectionView::Shown() const {
  const base::Value* result = GetActiveUserPrefService()
                                  ->Get(prefs::kLauncherFilesPrivacyNotice)
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

  // When hiding the launcher, stop the privacy notice shown timer to restart
  // the count on the next show.
  if (!shown && privacy_notice_shown_timer_.IsRunning())
    privacy_notice_shown_timer_.AbandonAndStop();

  UpdateElementsVisibility();
}

void ContinueSectionView::SetPrivacyNoticeAcceptedForTest(bool is_disabled) {
  g_nudge_accepted_for_test = is_disabled;
}

BEGIN_METADATA(ContinueSectionView, views::View)
END_METADATA

}  // namespace ash
