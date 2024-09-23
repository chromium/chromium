// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_task_container_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/common/constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view_class_properties.h"

using views::BoxLayout;
using views::FlexLayout;
using views::TableLayout;

namespace ash {
namespace {
// Suggested tasks layout constants.
constexpr int kColumnInnerSpacingClamshell = 8;
constexpr int kColumnOuterSpacingClamshell = 6;
constexpr int kColumnSpacingTablet = 16;
constexpr int kRowSpacing = 8;
constexpr size_t kMaxFilesForContinueSection = 4;

std::vector<SearchResult*> GetTasksResultsForContinueSection(
    SearchModel::SearchResults* results) {
  auto continue_filter = [](const SearchResult& r) -> bool {
    return r.display_type() == SearchResultDisplayType::kContinue;
  };
  std::vector<SearchResult*> continue_results;
  continue_results = SearchModel::FilterSearchResultsByFunction(
      results, base::BindRepeating(continue_filter),
      /*max_results=*/4);

  return continue_results;
}

// Fades out continue task view `view` from the container.
void ScheduleFadeOutAnimation(views::View* view,
                              views::AnimationSequenceBlock* sequence) {
  // Animate views for results that have been removed.
  // Opacity changes 100% -> 0%, while the size changes from 100% -> 75%
  // original size.
  gfx::Transform scale;
  scale.Scale(0.75f, 0.75f);
  sequence->SetTransform(
      view->layer(),
      gfx::TransformAboutPivot(gfx::RectF(view->GetLocalBounds()).CenterPoint(),
                               scale),
      gfx::Tween::FAST_OUT_LINEAR_IN);
  sequence->SetOpacity(view->layer(), 0.0f, gfx::Tween::FAST_OUT_LINEAR_IN);
}

// Slides (and fades) in a new result view into the task container.
// The view is translated from right into target position while animating
// opacity from 1 -> 0. `offfset` is the initial horizontal translation from
// which the view will slide in the target position. The offset direction is
// flipped if `is_rtl` is set.
void ScheduleSlideInAnimation(views::View* view,
                              int offset,
                              bool is_rtl,
                              views::AnimationSequenceBlock* sequence) {
  gfx::Transform initial_translate;
  initial_translate.Translate(offset * (is_rtl ? -1 : 1), 0);
  view->layer()->SetTransform(initial_translate);
  sequence->SetTransform(view->layer(), gfx::Transform(),
                         gfx::Tween::ACCEL_LIN_DECEL_100_3);

  view->layer()->SetOpacity(0.0f);
  sequence->SetOpacity(view->layer(), 1.0f, gfx::Tween::ACCEL_LIN_DECEL_100_3);
}

// Slides (and fades) out an old result views from the task container.
// The view is translated from its current position to the left, while animating
// opacity from 1 -> 0. `offfset` is the target view's horizontal translation
// from the initial position. The offset direction is flipped if `is_rtl` is
// set.
void ScheduleSlideOutAnimation(views::View* view,
                               int offset,
                               bool is_rtl,
                               views::AnimationSequenceBlock* sequence) {
  gfx::Transform target_translate;
  target_translate.Translate(offset * (is_rtl ? -1 : 1), 0);

  sequence->SetTransform(view->layer(), target_translate,
                         gfx::Tween::FAST_OUT_LINEAR_IN);
  sequence->SetOpacity(view->layer(), 0.0f, gfx::Tween::FAST_OUT_LINEAR_IN);
}

}  // namespace

ContinueTaskContainerView::ContinueTaskContainerView(
    AppListViewDelegate* view_delegate,
    int columns,
    OnResultsChanged update_callback,
    bool tablet_mode)
    : view_delegate_(view_delegate),
      update_callback_(update_callback),
      tablet_mode_(tablet_mode) {
  DCHECK(!update_callback_.is_null());

  if (tablet_mode_) {
    InitializeTabletLayout();
  } else {
    columns_ = columns;
    InitializeClamshellLayout();
  }
  GetViewAccessibility().SetRole(ax::mojom::Role::kList);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_ASH_LAUNCHER_CONTINUE_SECTION_LABEL),
      ax::mojom::NameFrom::kAttribute);
}

ContinueTaskContainerView::~ContinueTaskContainerView() = default;

void ContinueTaskContainerView::ListItemsAdded(size_t start, size_t count) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::ListItemsRemoved(size_t start, size_t count) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::ListItemMoved(size_t index,
                                              size_t target_index) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::ListItemsChanged(size_t start, size_t count) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::VisibilityChanged(views::View* starting_from,
                                                  bool is_visible) {
  if (!is_visible) {
    AbortTasksUpdateAnimations();
  } else {
    animations_timer_.Start(FROM_HERE, base::Seconds(2), base::DoNothing());
  }

  auto* notifier = view_delegate_->GetNotifier();
  if (notifier) {
    // NOTE: Use `IsDrawn()` instead of `is_visible` to account for parent
    // container visibility - `IsDrawn()` will return false if this view is
    // visible but its parent is not.
    notifier->NotifyContinueSectionVisibilityChanged(
        SearchResultDisplayType::kContinue, IsDrawn());
  }
}

bool ContinueTaskContainerView::OnKeyPressed(const ui::KeyEvent &event) {
  // No special focus handling in tablet mode.
  if (tablet_mode_) {
    return false;
  }
  if (event.key_code() == ui::VKEY_UP) {
    MoveFocusUp();
    return true;
  }
  if (event.key_code() == ui::VKEY_DOWN) {
    MoveFocusDown();
    return true;
  }
  return false;
}

void ContinueTaskContainerView::Update() {
  // Invalidate this callback to cancel a scheduled update.
  update_factory_.InvalidateWeakPtrs();
  AbortTasksUpdateAnimations();

  std::vector<SearchResult*> tasks =
      GetTasksResultsForContinueSection(results_);

  // Collect updated set of result IDs, which will be used to determine which
  // views need to be animated.
  std::vector<std::string> new_ids;
  for (const SearchResult* task : tasks) {
    new_ids.push_back(task->id());
  }

  // Only animate container contents update - when continue section is being
  // initialized, show the contents immediately.
  const bool first_show = animations_timer_.IsRunning() || !GetWidget() ||
                          !IsDrawn() || suggestion_tasks_views_.empty();

  std::set<views::View*> views_to_fade_out;
  std::map<std::string, views::View*> views_to_slide_out;
  std::map<std::string, views::View*> views_remaining_in_place;

  // Determine whether an animation is needed and gather information needed to
  // configure update animation.
  const bool chip_count_changed =
      tasks.size() != suggestion_tasks_views_.size();
  bool needs_animation = !first_show && chip_count_changed;
  if (!first_show) {
    for (size_t i = 0; i < suggestion_tasks_views_.size(); ++i) {
      ContinueTaskView* result_view = suggestion_tasks_views_[i];

      // Some views may be kept around during update animation, so they can
      // animate out - remove the from layout manager so they don't affect new
      // layout.
      RemoveViewFromLayout(result_view);

      TaskViewRemovalAnimation animation =
          GetRemovalAnimationForTaskView(result_view, i, new_ids);
      switch (animation) {
        case TaskViewRemovalAnimation::kFadeOut:
          views_to_fade_out.insert(result_view);
          break;
        case TaskViewRemovalAnimation::kSlideOut:
          views_to_slide_out.emplace(result_view->result()->id(), result_view);
          break;
        case TaskViewRemovalAnimation::kNone:
          // In tablet mode, if the number of chips has changed, the chip bounds
          // and size are likely to change, so slide out existing items even if
          // they remain at the same logical position in the container.
          if (tablet_mode_ && chip_count_changed) {
            views_to_slide_out.emplace(result_view->result()->id(),
                                       result_view);
          } else {
            views_remaining_in_place.emplace(result_view->result()->id(),
                                             result_view);
          }
          break;
      }

      // Unless the result view remains in the same position within the task
      // container, the task update requires animation.
      if (animation != TaskViewRemovalAnimation::kNone)
        needs_animation = true;
    }
  }

  if (needs_animation) {
    views_to_remove_after_animation_.swap(suggestion_tasks_views_);
  } else {
    // When not animating, all views can be removed immediately.
    RemoveAllChildViews();
  }

  suggestion_tasks_views_.clear();

  num_results_ = std::min(kMaxFilesForContinueSection, tasks.size());

  num_file_results_ = 0;
  num_desks_admin_template_results_ = 0;
  for (size_t i = 0; i < num_results_; ++i) {
    if (tasks[i]->result_type() == AppListSearchResultType::kZeroStateFile ||
        tasks[i]->result_type() == AppListSearchResultType::kZeroStateDrive) {
      ++num_file_results_;
    }

    if (tasks[i]->result_type() ==
        AppListSearchResultType::kDesksAdminTemplate) {
      ++num_desks_admin_template_results_;
    }
  }

  // Create new result views.
  for (size_t i = 0; i < num_results_; ++i) {
    auto task =
        std::make_unique<ContinueTaskView>(view_delegate_, tablet_mode_);
    if (i == 0)
      task->SetProperty(views::kMarginsKey, gfx::Insets());
    task->set_index_in_container(i);
    task->SetResult(tasks[i]);
    suggestion_tasks_views_.emplace_back(task.get());
    AddChildView(std::move(task));
  }

  // Layout the container so the task bounds are set to their intended
  // positions, which will be used to configure container update animation
  // sequences when animating.
  DeprecatedLayoutImmediately();

  if (needs_animation) {
    ScheduleContainerUpdateAnimation(views_to_fade_out, views_to_slide_out,
                                     views_remaining_in_place);
  }

  auto* notifier = view_delegate_->GetNotifier();
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* task : tasks)
      notifier_results.emplace_back(task->id(), task->metrics_type(),
                                    task->continue_file_suggestion_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kContinue,
                                   notifier_results);
  }
  if (!update_callback_.is_null())
    update_callback_.Run();
}

ContinueTaskContainerView::TaskViewRemovalAnimation
ContinueTaskContainerView::GetRemovalAnimationForTaskView(
    ContinueTaskView* task_view,
    size_t old_index,
    const std::vector<std::string>& new_task_ids) {
  // If the result associated with the result was reset, animate the view
  // out.
  if (!task_view->result())
    return TaskViewRemovalAnimation::kFadeOut;

  const std::string& task_id = task_view->result()->id();
  auto new_ids_it = base::ranges::find(new_task_ids, task_id);

  // If the associated result was removed from the task list, animate it out.
  if (new_ids_it == new_task_ids.end())
    return TaskViewRemovalAnimation::kFadeOut;

  const size_t new_index = (new_ids_it - new_task_ids.begin());

  if (old_index != new_index)
    return TaskViewRemovalAnimation::kSlideOut;

  return TaskViewRemovalAnimation::kNone;
}

void ContinueTaskContainerView::ScheduleContainerUpdateAnimation(
    const std::set<views::View*>& views_to_fade_out,
    const std::map<std::string, views::View*>& views_to_slide_out,
    const std::map<std::string, views::View*>& views_remaining_in_place) {
  views::AnimationBuilder animation_builder;
  animation_builder.OnEnded(base::BindOnce(
      &ContinueTaskContainerView::ClearAnimatingViews, base::Unretained(this)));
  animation_builder.OnAborted(base::BindOnce(
      &ContinueTaskContainerView::ClearAnimatingViews, base::Unretained(this)));

  animation_builder.Once().SetDuration(base::Milliseconds(100));

  // Fade out views for results that got removed.
  for (auto* view : views_to_fade_out)
    ScheduleFadeOutAnimation(view, &animation_builder.GetCurrentSequence());

  // Immediately hide views that remained in place, and for which the new result
  // views will not be animated in.
  for (auto& view : views_remaining_in_place)
    view.second->SetVisible(false);

  const bool is_rtl = base::i18n::IsRTL();

  // Slide out old result views for results whose position changed.
  base::TimeDelta delay =
      views_to_fade_out.empty() ? base::TimeDelta() : base::Milliseconds(200);
  animation_builder.GetCurrentSequence().At(delay).SetDuration(
      base::Milliseconds(100));
  for (auto& view : views_to_slide_out) {
    ScheduleSlideOutAnimation(view.second, tablet_mode_ ? 0 : -34, is_rtl,
                              &animation_builder.GetCurrentSequence());
  }

  // Animate new views in.
  delay = views_to_fade_out.empty() ? base::Milliseconds(100)
                                    : base::Milliseconds(300);
  animation_builder.GetCurrentSequence().At(delay).SetDuration(
      base::Milliseconds(300));

  for (ash::ContinueTaskView* view : suggestion_tasks_views_) {
    const std::string& result_id = view->result()->id();
    // If view remained in place, it does not need to be animated in.
    auto view_remaining_in_place_it = views_remaining_in_place.find(result_id);
    if (view_remaining_in_place_it != views_remaining_in_place.end())
      continue;

    int initial_offset = 60;
    // In tablet mode, direction from which the view slides in depends on
    // whether the view is coming in from left or right - if the result existed
    // before the update, and its old view bounds were left of the new view
    // bounds, slide the view in from the left by flipping offset direction.
    if (tablet_mode_) {
      const auto& old_view_it = views_to_slide_out.find(result_id);
      if (old_view_it != views_to_slide_out.end() &&
          old_view_it->second->x() < view->x()) {
        initial_offset = -initial_offset;
      }
    }
    ScheduleSlideInAnimation(view, initial_offset, is_rtl,
                             &animation_builder.GetCurrentSequence());
  }
}

void ContinueTaskContainerView::AbortTasksUpdateAnimations() {
  for (ash::ContinueTaskView* view : suggestion_tasks_views_) {
    view->layer()->GetAnimator()->StopAnimating();
  }
  ClearAnimatingViews();
}

void ContinueTaskContainerView::ClearAnimatingViews() {
  // Clear `views_to_remove_after_animation_` before starting to remove views in
  // case view removal causes an aborted view animation that calls back into
  // `ClearAnimatingViews()`. Clearing `views_to_remove_after_animation_` mid
  // iteraion over the vector would not be safe.
  std::vector<raw_ptr<ContinueTaskView, VectorExperimental>> views_to_remove;
  views_to_remove_after_animation_.swap(views_to_remove);
  for (ash::ContinueTaskView* view : views_to_remove) {
    RemoveChildViewT(view);
  }

  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged, true);
}

void ContinueTaskContainerView::SetResults(
    SearchModel::SearchResults* results) {
  list_model_observation_.Reset();

  results_ = results;
  if (results_)
    list_model_observation_.Observe(results);

  Update();
}

void ContinueTaskContainerView::DisableFocusForShowingActiveFolder(
    bool disabled) {
  for (views::View* child : suggestion_tasks_views_)
    child->SetEnabled(!disabled);
}

void ContinueTaskContainerView::AnimateSlideInSuggestions(
    int available_space,
    base::TimeDelta duration,
    gfx::Tween::Type tween) {
  SetVisible(true);

  const int rows =
      columns_ ? std::ceil(static_cast<double>(suggestion_tasks_views_.size()) /
                           columns_)
               : 1;
  double space_per_row = static_cast<double>(available_space) / rows;

  for (size_t i = 0; i < suggestion_tasks_views_.size(); i++) {
    views::View* view = suggestion_tasks_views_[i];
    gfx::Transform translation;

    int row_number = columns_ ? ((i / columns_) + 1) : 1;
    // Distribute the space between the elements so that the space between the
    // previous element in the parent view and the first row is the same as the
    // space between rows. The items in the first row will just be translated by
    // `space_per_row`. The items from the second row need to carry over
    // the space translated by the first row and translate again
    // `space_per_row` to have even space between elements.
    translation.Translate(0, space_per_row * row_number);

    view->layer()->SetTransform(translation);
    view->layer()->SetOpacity(0.0f);
  }
  views::AnimationBuilder animation_builder;
  animation_builder.Once().SetDuration(duration);

  for (ash::ContinueTaskView* view : suggestion_tasks_views_) {
    animation_builder.GetCurrentSequence()
        .SetTransform(view, gfx::Transform(), tween)
        .SetOpacity(view, 1.0f, tween);
  }
}

void ContinueTaskContainerView::RemoveViewFromLayout(ContinueTaskView* view) {
  view->SetEnabled(false);
  view->SetProperty(views::kViewIgnoredByLayoutKey, true);
}

void ContinueTaskContainerView::ScheduleUpdate() {
  // When search results are added one by one, each addition generates an update
  // request. Consolidates those update requests into one Update call.
  if (!update_factory_.HasWeakPtrs()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ContinueTaskContainerView::Update,
                                  update_factory_.GetWeakPtr()));
  }
}

void ContinueTaskContainerView::InitializeTabletLayout() {
  DCHECK(tablet_mode_);
  DCHECK(!columns_);

  SetLayoutManager(std::make_unique<FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::TLBR(0, kColumnSpacingTablet, 0, 0))
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::LayoutOrientation::kHorizontal,
                      views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                      views::MaximumFlexSizeRule::kScaleToMaximum));
}

void ContinueTaskContainerView::InitializeClamshellLayout() {
  DCHECK(!tablet_mode_);
  DCHECK_GT(columns_, 0);

  auto* const table_layout =
      SetLayoutManager(std::make_unique<views::TableLayout>());
  std::vector<size_t> linked_columns;
  for (int i = 0; i < columns_; i++) {
    if (i == 0) {
      table_layout->AddPaddingColumn(views::TableLayout::kFixedSize,
                                     kColumnOuterSpacingClamshell);
    } else {
      table_layout->AddPaddingColumn(views::TableLayout::kFixedSize,
                                     kColumnInnerSpacingClamshell);
    }
    table_layout->AddColumn(
        views::LayoutAlignment::kStretch, views::LayoutAlignment::kCenter,
        /*horizontal_resize=*/1.0f, views::TableLayout::ColumnSize::kFixed,
        /*fixed_width=*/0, /*min_width=*/0);
    linked_columns.push_back(2 * i + 1);
  }
  table_layout->AddPaddingColumn(views::TableLayout::kFixedSize,
                                 kColumnOuterSpacingClamshell);

  table_layout->LinkColumnSizes(linked_columns);
  // Continue section only shows if there are 3 or more suggestions, so there
  // are always 2 rows.
  table_layout->AddRows(1, views::TableLayout::kFixedSize);
  table_layout->AddPaddingRow(views::TableLayout::kFixedSize, kRowSpacing);
  table_layout->AddRows(1, views::TableLayout::kFixedSize);
}

void ContinueTaskContainerView::MoveFocusUp() {
  DVLOG(1) << __FUNCTION__;
  // This function should only run when a child has focus.
  DCHECK(Contains(GetFocusManager()->GetFocusedView()));
  DCHECK(!suggestion_tasks_views_.empty());
  int focused_index = GetIndexOfFocusedTaskView();
  DCHECK_GE(focused_index, 0);
  // Try to move up by one row.
  int target_index = focused_index - columns_;
  // If that would move before the first item, focus the first item and reverse
  // focus out of the section.
  if (target_index < 0) {
    suggestion_tasks_views_[0]->RequestFocus();
    GetFocusManager()->AdvanceFocus(/*reverse=*/true);
    return;
  }
  suggestion_tasks_views_[target_index]->RequestFocus();
}

void ContinueTaskContainerView::MoveFocusDown() {
  DVLOG(1) << __FUNCTION__;
  // This function should only run when a child has focus.
  DCHECK(Contains(GetFocusManager()->GetFocusedView()));
  DCHECK(!suggestion_tasks_views_.empty());
  int focused_index = GetIndexOfFocusedTaskView();
  DCHECK_GE(focused_index, 0);
  // Try to move down by one row.
  int target_index = focused_index + columns_;
  // If that would move past the last item, focus the last item and advance
  // focus out of the section.
  if (target_index >= static_cast<int>(suggestion_tasks_views_.size())) {
    suggestion_tasks_views_.back()->RequestFocus();
    GetFocusManager()->AdvanceFocus(/*reverse=*/false);
    return;
  }
  suggestion_tasks_views_[target_index]->RequestFocus();
}

int ContinueTaskContainerView::GetIndexOfFocusedTaskView() const {
  for (size_t i = 0; i < suggestion_tasks_views_.size(); ++i) {
    if (suggestion_tasks_views_[i]->HasFocus())
      return i;
  }
  return -1;
}

BEGIN_METADATA(ContinueTaskContainerView)
END_METADATA

}  // namespace ash
