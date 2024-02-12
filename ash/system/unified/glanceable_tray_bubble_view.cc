// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include <memory>
#include <numeric>

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
#include "base/ranges/algorithm.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/highlight_border.h"

namespace ash {

using BoundsType = CalendarView::CalendarSlidingSurfaceBoundsType;

namespace {

// If display height is greater than `kDisplayHeightThreshold`, the height of
// the `calendar_view_` is `kCalendarBubbleHeightLargeDisplay`, otherwise
// is `kCalendarBubbleHeightSmallDisplay`.
constexpr int kDisplayHeightThreshold = 800;
constexpr int kCalendarBubbleHeightSmallDisplay = 340;
constexpr int kCalendarBubbleHeightLargeDisplay = 368;

// Tasks Glanceables constants.
constexpr int kGlanceablesContainerCornerRadius = 24;

// The margin between each glanceable views.
constexpr int kMarginBetweenGlanceables = 8;

// The container view of time management glanceables, which includes Tasks and
// Classroom.
class TimeManagementContainer : public views::FlexLayoutView {
  METADATA_HEADER(TimeManagementContainer, views::FlexLayoutView)

 public:
  TimeManagementContainer() {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    SetOrientation(views::LayoutOrientation::kVertical);
    SetInteriorMargin(gfx::Insets(12));
    SetBackground(views::CreateThemedRoundedRectBackground(
        cros_tokens::kCrosSysSystemBaseElevated,
        kGlanceablesContainerCornerRadius));
    SetBorder(std::make_unique<views::HighlightBorder>(
        kGlanceablesContainerCornerRadius,
        views::HighlightBorder::Type::kHighlightBorderOnShadow));
    SetDefault(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));
  }
  TimeManagementContainer(const TimeManagementContainer&) = delete;
  TimeManagementContainer& operator=(const TimeManagementContainer&) = delete;
  ~TimeManagementContainer() override = default;

  void ChildPreferredSizeChanged(views::View* child) override {
    PreferredSizeChanged();
  }

  void ChildVisibilityChanged(views::View* child) override {
    PreferredSizeChanged();
  }
};

BEGIN_METADATA(TimeManagementContainer)
END_METADATA

// The view that parents glanceable bubbles. It's a flex layout view that
// propagates child preferred size changes to the tray bubble view and the
// container bounds changes to the bubble view.
class ContainerView : public views::FlexLayoutView,
                      public views::FocusChangeListener {
  METADATA_HEADER(ContainerView, views::FlexLayoutView)

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

    for (size_t i = 0; i < children().size(); ++i) {
      views::View* child = children()[i];
      child->SetProperty(
          views::kMarginsKey,
          gfx::Insets::TLBR(
              i == 0u ? 0 : kMarginBetweenGlanceables, 0,
              i == children().size() - 1 ? 0 : kMarginBetweenGlanceables, 0));
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
    for (views::View* child : children()) {
      if (child->Contains(view)) {
        return child;
      }
    }
    return nullptr;
  }

  base::RepeatingClosure preferred_size_change_callback_;
  HeightChangeCallback height_change_callback_;
};

BEGIN_METADATA(ContainerView)
END_METADATA

}  // namespace

GlanceableTrayBubbleView::GlanceableTrayBubbleView(
    const InitParams& init_params,
    Shelf* shelf)
    : TrayBubbleView(init_params), shelf_(shelf) {
  Shell::Get()->glanceables_controller()->RecordGlanceablesBubbleShowTime(
      base::TimeTicks::Now());
  // The calendar view should always keep its size if possible. If there is no
  // enough space, the `scroll_view_` and `time_management_container_view_`
  // should be prioritized to be shrunk. Set the default flex to 0 and manually
  // updates the flex of views depending on the view hierarchy.
  box_layout()->SetDefaultFlex(0);
  box_layout()->set_between_child_spacing(kMarginBetweenGlanceables);
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
    if (is_calendar_for_glanceables) {
      calendar_container_ =
          AddChildView(std::make_unique<views::FlexLayoutView>());
    }

    auto* calendar_parent_view = is_calendar_for_glanceables
                                     ? calendar_container_
                                     : scroll_view_->contents();
    calendar_view_ = calendar_parent_view->AddChildView(
        std::make_unique<CalendarView>(/*for_glanceables_container=*/true));
    SetCalendarPreferredSize();
  }

  auto* const tasks_client =
      Shell::Get()->glanceables_controller()->GetTasksClient();
  if (should_show_non_calendar_glanceables && tasks_client) {
    CHECK(!tasks_bubble_view_);
    tasks_client->GetTaskLists(
        /*force_fetch=*/false,
        base::BindOnce(&GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  const int max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  SetMaxHeight(max_height);
  ChangeAnchorAlignment(shelf_->alignment());
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());

  if (!features::AreAnyGlanceablesTimeManagementViewsEnabled()) {
    auto* const classroom_client =
        Shell::Get()->glanceables_controller()->GetClassroomClient();
    if (should_show_non_calendar_glanceables && classroom_client) {
      if (!classroom_bubble_student_view_) {
        classroom_client->IsStudentRoleActive(base::BindOnce(
            &GlanceableTrayBubbleView::AddClassroomBubbleStudentViewIfNeeded,
            weak_ptr_factory_.GetWeakPtr()));
      }
    }
  }
  calendar_view_->ScrollViewToVisible();

  ClipScrollViewHeight(screen_max_height);

  // Layout to set the calendar view bounds, so the calendar view finishes
  // initializing (e.g. scroll to today), which happens when the calendar view
  // bounds are set.
  DeprecatedLayoutImmediately();

  initialized_ = true;
}

int GlanceableTrayBubbleView::GetHeightForWidth(int width) const {
  // Let the layout manager calculate the preferred height instead of using the
  // one from TrayBubbleView, which doesn't take the layout manager and margin
  // settings into consider.
  return std::min(views::View::GetHeightForWidth(width),
                  CalculateMaxTrayBubbleHeight(shelf_->GetWindow()));
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
      base::ranges::find(scroll_contents->children(), calendar_view_) -
      scroll_contents->children().begin();
  classroom_bubble_student_view_ = scroll_contents->AddChildViewAt(
      std::make_unique<ClassroomBubbleStudentView>(), calendar_view_index);

  AdjustChildrenFocusOrder();
}

void GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded(
    bool fetch_success,
    const ui::ListModel<api::TaskList>* task_lists) {
  if (task_lists->item_count() == 0) {
    return;
  }
  // Add tasks bubble before everything.
  if (features::IsGlanceablesTimeManagementTasksViewEnabled()) {
    time_management_container_view_ =
        AddChildViewAt(std::make_unique<TimeManagementContainer>(), 0);
    box_layout()->SetFlexForView(time_management_container_view_, 1);
    tasks_bubble_view_ = time_management_container_view_->AddChildView(
        std::make_unique<GlanceablesTasksView>(task_lists));
    UpdateBubble();
  } else {
    tasks_bubble_view_ = scroll_view_->contents()->AddChildViewAt(
        std::make_unique<TasksBubbleView>(task_lists), 0);
    box_layout()->SetFlexForView(scroll_view_, 1);
  }

  AdjustChildrenFocusOrder();
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
      features::AreAnyGlanceablesTimeManagementViewsEnabled()) {
    return;
  }

  scroll_view_->ScrollByOffset(gfx::PointF(0, -height_delta));
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view && scroll_view_->contents()->Contains(focused_view)) {
    focused_view->ScrollViewToVisible();
  }
}

void GlanceableTrayBubbleView::AdjustChildrenFocusOrder() {
  const bool is_calendar_for_glanceables =
      features::IsGlanceablesV2CalendarViewEnabled();

  // Make sure the view that contains calendar is the first in the focus list of
  // glanceable views. Depending on whether GlanceablesV2CalendarView is
  // enabled, the nearest common ancestor of the calendar view and other
  // glanceables is `this`, or `scroll_view_->contents()`.
  if (is_calendar_for_glanceables) {
    auto* default_focused_child = GetChildrenFocusList().front().get();
    if (default_focused_child != calendar_container_) {
      calendar_container_->InsertBeforeInFocusList(default_focused_child);
    }
  } else {
    auto* default_focused_child =
        scroll_view_->contents()->GetChildrenFocusList().front().get();
    if (default_focused_child != calendar_view_) {
      calendar_view_->InsertBeforeInFocusList(default_focused_child);
    }
  }

  const bool time_management_stable_launch =
      features::AreAnyGlanceablesTimeManagementViewsEnabled();

  // Only adds the time management view/container after the calendar
  // view/container in the focus list if the calendar flag and the time
  // management flag are on or off at the same time. Otherwise one of them will
  // be in the scroll view and the other will be at the same level of the scroll
  // view.
  if (is_calendar_for_glanceables != time_management_stable_launch) {
    return;
  }

  if (time_management_stable_launch) {
    time_management_container_view_->InsertAfterInFocusList(
        calendar_container_);
  } else {
    tasks_bubble_view_->InsertAfterInFocusList(calendar_view_);
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
                                    kMarginBetweenGlanceables);
}

BEGIN_METADATA(GlanceableTrayBubbleView)
END_METADATA

}  // namespace ash
