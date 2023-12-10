// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include <algorithm>
#include <memory>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/classroom_bubble_student_view.h"
#include "ash/system/unified/tasks_bubble_view.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

using BoundsType = CalendarView::CalendarSlidingSurfaceBoundsType;

namespace {

// If display height is greater than `kDisplayHeightThreshold`, the height of
// the `calendar_view_` is `kCalendarBubbleHeightLargeDisplay`, otherwise
// is `kCalendarBubbleHeightSmallDisplay`.
constexpr int kDisplayHeightThreshold = 800;
constexpr int kCalendarBubbleHeightSmallDisplay = 340;
constexpr int kCalendarBubbleHeightLargeDisplay = 368;

// For Calendar for Glanceables:
constexpr auto kCalendarBubbleBorder = gfx::Insets::TLBR(8, 0, 0, 0);

// The view that parents glanceable bubbles. It's a flex layout view that
// propagates child preferred size changes to the tray bubble view and the
// container bounds changes to the bubble view.
class ContainerView : public views::FlexLayoutView,
                      public views::FocusChangeListener {
 public:
  using HeightChangeCallback = base::RepeatingCallback<void(int height_delta)>;
  ContainerView(const base::RepeatingClosure& preferred_size_change_callback,
                const HeightChangeCallback& height_change_callback)
      : preferred_size_change_callback_(preferred_size_change_callback),
        height_change_callback_(height_change_callback) {
    SetOrientation(views::LayoutOrientation::kVertical);
    SetCollapseMargins(true);
  }

  ContainerView(const ContainerView&) = delete;
  ContainerView& operator=(const ContainerView&) = delete;
  ~ContainerView() override = default;

  // views::FlexLayoutView:
  void ChildPreferredSizeChanged(views::View* child) override {
    views::FlexLayoutView::ChildPreferredSizeChanged(child);
    preferred_size_change_callback_.Run();
  }

  void ChildVisibilityChanged(views::View* child) override {
    views::FlexLayoutView::ChildPreferredSizeChanged(child);
    preferred_size_change_callback_.Run();
  }

  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override {
    views::FlexLayoutView::ViewHierarchyChanged(details);

    const int kDefaultMargin = 8;
    for (size_t i = 0; i < children().size(); ++i) {
      views::View* child = children()[i];
      child->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(i == 0u ? 0 : kDefaultMargin, 0,
                            i == children().size() - 1 ? 0 : kDefaultMargin,
                            0));
    }

    if (details.parent == this && details.child->GetVisible()) {
      preferred_size_change_callback_.Run();
    }
  }

  void PreferredSizeChanged() override {
    views::FlexLayoutView::PreferredSizeChanged();
    preferred_size_change_callback_.Run();
  }

  void OnBoundsChanged(const gfx::Rect& old_bounds) override {
    views::FlexLayoutView::OnBoundsChanged(old_bounds);

    const int height_delta = old_bounds.height() - bounds().height();
    if (height_delta != 0) {
      height_change_callback_.Run(height_delta);
    }
  }

  void AddedToWidget() override {
    GetFocusManager()->AddFocusChangeListener(this);
  }

  void RemovedFromWidget() override {
    GetFocusManager()->RemoveFocusChangeListener(this);
  }

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override {
    views::View* container_for_new_focus = GetChildThatContains(focused_now);
    // It the focus is moving into a glanceable container, try scrolling the
    // whole container into the viewport.
    if (container_for_new_focus &&
        container_for_new_focus != GetChildThatContains(focused_before)) {
      container_for_new_focus->ScrollViewToVisible();
    }
  }
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override {}

 private:
  views::View* GetChildThatContains(views::View* view) {
    for (auto* child : children()) {
      if (child->Contains(view)) {
        return child;
      }
    }
    return nullptr;
  }

  base::RepeatingClosure preferred_size_change_callback_;
  HeightChangeCallback height_change_callback_;
};

}  // namespace

GlanceableTrayBubbleView::GlanceableTrayBubbleView(
    const InitParams& init_params,
    Shelf* shelf)
    : TrayBubbleView(init_params), shelf_(shelf) {
  Shell::Get()->glanceables_controller()->RecordGlanceablesBubbleShowTime(
      base::TimeTicks::Now());
}

GlanceableTrayBubbleView::~GlanceableTrayBubbleView() {
  Shell::Get()->glanceables_controller()->NotifyGlanceablesBubbleClosed();
}

void GlanceableTrayBubbleView::InitializeContents() {
  CHECK(!initialized_);

  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->SetPaintToLayer();
  scroll_view_->layer()->SetFillsBoundsOpaquely(false);
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetBackgroundColor(std::nullopt);
  scroll_view_->layer()->SetIsFastRoundedCorner(true);
  scroll_view_->SetDrawOverflowIndicator(false);
  scroll_view_->SetVerticalScrollBarMode(
      views::ScrollView::ScrollBarMode::kHiddenButEnabled);

  // TODO(b/286941809): Apply rounded corners. Temporary removed because they
  // make the background blur to disappear and this requires further
  // investigation.

  const bool is_calendar_for_glanceables =
      features::IsGlanceablesV2CalendarViewEnabled();

  // Adjusts the calendar sliding surface bounds (`UpNextView`) with the
  // glanceable view's scrolling. If `kGlanceablesV2CalendarView` is enabled,
  // this is not needed since `calendar_view_` will be in a separate bubble.
  if (!is_calendar_for_glanceables) {
    on_contents_scrolled_subscription_ =
        scroll_view_->AddContentsScrolledCallback(base::BindRepeating(
            [](GlanceableTrayBubbleView* bubble) {
              if (!bubble || !bubble->calendar_view_ ||
                  bubble->calendar_view_->event_list_view()) {
                return;
              }
              bubble->calendar_view_->SetCalendarSlidingSurfaceBounds(
                  bubble->calendar_view_->up_next_view()
                      ? BoundsType::UP_NEXT_VIEW_BOUNDS
                      : BoundsType::CALENDAR_BOTTOM_BOUNDS);
            },
            base::Unretained(this)));
  }

  auto child_glanceable_container = std::make_unique<ContainerView>(
      base::BindRepeating(
          &GlanceableTrayBubbleView::OnGlanceablesContainerPreferredSizeChanged,
          base::Unretained(this)),
      base::BindRepeating(
          &GlanceableTrayBubbleView::OnGlanceablesContainerHeightChanged,
          base::Unretained(this)));

  const auto* const session_controller = Shell::Get()->session_controller();
  CHECK(session_controller);
  const bool should_show_non_calendar_glanceables =
      session_controller->IsActiveUserSessionStarted() &&
      session_controller->GetSessionState() ==
          session_manager::SessionState::ACTIVE &&
      session_controller->GetUserSession(0)->user_info.has_gaia_account;

  scroll_view_->SetContents(std::move(child_glanceable_container));

  const int screen_max_height =
      CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  if (!calendar_view_) {
    auto* calendar_parent_view = scroll_view_->contents();
    if (is_calendar_for_glanceables) {
      calendar_parent_view =
          AddChildView(std::make_unique<views::FlexLayoutView>());
      calendar_parent_view->SetBorder(
          views::CreateEmptyBorder(kCalendarBubbleBorder));
    }

    calendar_view_ = calendar_parent_view->AddChildView(
        std::make_unique<CalendarView>(/*for_glanceables_container=*/true));
    SetCalendarPreferredSize();
  }

  auto* const tasks_client =
      Shell::Get()->glanceables_controller()->GetTasksClient();
  if (should_show_non_calendar_glanceables && tasks_client) {
    CHECK(!tasks_bubble_view_);
    tasks_client->GetTaskLists(
        base::BindOnce(&GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  const int max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  SetMaxHeight(max_height);
  ChangeAnchorAlignment(shelf_->alignment());
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());

  auto* const classroom_client =
      Shell::Get()->glanceables_controller()->GetClassroomClient();
  if (should_show_non_calendar_glanceables && classroom_client) {
    if (!classroom_bubble_student_view_) {
      classroom_client->IsStudentRoleActive(base::BindOnce(
          &GlanceableTrayBubbleView::AddClassroomBubbleStudentViewIfNeeded,
          weak_ptr_factory_.GetWeakPtr()));
    }
  }

  calendar_view_->ScrollViewToVisible();

  ClipScrollViewHeight(screen_max_height);

  // Layout to set the calendar view bounds, so the calendar view finishes
  // initializing (e.g. scroll to today), which happens when the calendar view
  // bounds are set.
  Layout();

  initialized_ = true;
}

void GlanceableTrayBubbleView::AddedToWidget() {
  if (!initialized_) {
    InitializeContents();
  }
  TrayBubbleView::AddedToWidget();
}

void GlanceableTrayBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (tasks_bubble_view_) {
    tasks_bubble_view_->CancelUpdates();
  }
  if (classroom_bubble_student_view_) {
    classroom_bubble_student_view_->CancelUpdates();
  }

  TrayBubbleView::OnWidgetClosing(widget);
}

void GlanceableTrayBubbleView::OnDisplayConfigurationChanged() {
  int max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  SetMaxHeight(max_height);
  SetCalendarPreferredSize();
  ClipScrollViewHeight(max_height);
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());
}

void GlanceableTrayBubbleView::AddClassroomBubbleStudentViewIfNeeded(
    bool is_role_active) {
  if (!is_role_active) {
    return;
  }

  // Adds classroom bubble before `calendar_view_`.
  auto* const scroll_contents = scroll_view_->contents();
  const auto calendar_view_index =
      std::find(scroll_contents->children().begin(),
                scroll_contents->children().end(), calendar_view_) -
      scroll_contents->children().begin();
  classroom_bubble_student_view_ = scroll_contents->AddChildViewAt(
      std::make_unique<ClassroomBubbleStudentView>(), calendar_view_index);

  const bool is_calendar_for_glanceables =
      features::IsGlanceablesV2CalendarViewEnabled();

  // For Calendar for Glanceables: `calendar_view_` should be focused before the
  // `scroll_view_`. Otherwise `calendar_view_` should be focused before other
  // bubbles inside `scroll_view_`.
  views::View* const default_focused_child =
      is_calendar_for_glanceables
          ? GetChildrenFocusList().front()
          : scroll_contents->GetChildrenFocusList().front();
  if (default_focused_child != calendar_view_) {
    auto* target_view =
        is_calendar_for_glanceables ? calendar_view_->parent() : calendar_view_;
    target_view->InsertBeforeInFocusList(default_focused_child);
  }

  // Only adds `tasks_bubble_view_` after `calendar_view_` in the focus list if
  // `kGlanceablesCalendarView` is disabled since otherwise they are not at the
  // same level in the view hierarchy.
  if (!is_calendar_for_glanceables && tasks_bubble_view_) {
    tasks_bubble_view_->InsertAfterInFocusList(calendar_view_);
  }
}

void GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded(
    const ui::ListModel<api::TaskList>* task_lists) {
  if (task_lists->item_count() == 0) {
    return;
  }
  // Add tasks bubble before everything.
  auto* const scroll_contents = scroll_view_->contents();

  std::unique_ptr<GlanceablesTasksViewBase> view;
  if (base::FeatureList::IsEnabled(
          features::kGlanceablesTimeManagementStableLaunch)) {
    view = std::make_unique<GlanceablesTasksView>(task_lists);
  } else {
    view = std::make_unique<TasksBubbleView>(task_lists);
  }
  tasks_bubble_view_ = scroll_contents->AddChildViewAt(std::move(view), 0);

  const bool is_calendar_for_glanceables =
      features::IsGlanceablesV2CalendarViewEnabled();

  // For Calendar for Glanceables: `calendar_view_` should be focused before the
  // `scroll_view_`. Otherwise `calendar_view_` should be focused before other
  // bubbles inside `scroll_view_`.
  views::View* const default_focused_child =
      is_calendar_for_glanceables
          ? GetChildrenFocusList().front()
          : scroll_contents->GetChildrenFocusList().front();
  if (default_focused_child != calendar_view_) {
    auto* target_view =
        is_calendar_for_glanceables ? calendar_view_->parent() : calendar_view_;
    target_view->InsertBeforeInFocusList(default_focused_child);
  }

  // Only adds `tasks_bubble_view_` after `calendar_view_` in the focus list if
  // `kGlanceablesCalendarView` is disabled since otherwise they are not at the
  // same level in the view hierarchy.
  if (!is_calendar_for_glanceables) {
    tasks_bubble_view_->InsertAfterInFocusList(calendar_view_);
  }
}

void GlanceableTrayBubbleView::OnGlanceablesContainerPreferredSizeChanged() {
  if (!initialized_) {
    return;
  }

  UpdateBubble();
}

void GlanceableTrayBubbleView::OnGlanceablesContainerHeightChanged(
    int height_delta) {
  if (!initialized_ || !IsDrawn() || !GetWidget() || GetWidget()->IsClosed() ||
      base::FeatureList::IsEnabled(
          features::kGlanceablesTimeManagementStableLaunch)) {
    return;
  }

  scroll_view_->ScrollByOffset(gfx::PointF(0, -height_delta));
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view && scroll_view_->contents()->Contains(focused_view)) {
    focused_view->ScrollViewToVisible();
  }
}

void GlanceableTrayBubbleView::SetCalendarPreferredSize() const {
  const bool is_calendar_for_glanceables =
      features::IsGlanceablesV2CalendarViewEnabled();
  // TODO(b/312320532): Update the height if display height is less than
  // `kCalendarBubbleHeightSmallDisplay`.
  calendar_view_->SetPreferredSize(
      is_calendar_for_glanceables
          ? gfx::Size(kWideTrayMenuWidth,
                      CalculateMaxTrayBubbleHeight(shelf_->GetWindow()) >
                              kDisplayHeightThreshold
                          ? kCalendarBubbleHeightLargeDisplay
                          : kCalendarBubbleHeightSmallDisplay)
          : gfx::Size(kWideTrayMenuWidth, 400));
}

void GlanceableTrayBubbleView::ClipScrollViewHeight(
    int screen_max_height) const {
  if (!features::IsGlanceablesV2CalendarViewEnabled()) {
    return;
  }

  scroll_view_->ClipHeightTo(0, screen_max_height - calendar_view_->height() -
                                    kCalendarBubbleBorder.top());
}

BEGIN_METADATA(GlanceableTrayBubbleView, TrayBubbleView)
END_METADATA

}  // namespace ash
