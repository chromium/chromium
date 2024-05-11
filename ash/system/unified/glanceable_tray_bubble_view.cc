// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include <memory>
#include <numeric>
#include <vector>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/classroom/glanceables_classroom_types.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
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
    layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
    layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(kGlanceablesContainerCornerRadius));
    SetOrientation(views::LayoutOrientation::kVertical);

    // Set all inner margins and the spacing between children to 8.
    SetInteriorMargin(gfx::Insets(8));
    SetCollapseMargins(true);
    SetDefault(views::kMarginsKey, gfx::Insets::VH(8, 0));

    SetBackground(views::CreateThemedSolidBackground(
        cros_tokens::kCrosSysSystemBaseElevated));
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

}  // namespace

GlanceableTrayBubbleView::GlanceableTrayBubbleView(
    const InitParams& init_params,
    Shelf* shelf)
    : TrayBubbleView(init_params), shelf_(shelf) {
  Shell::Get()->glanceables_controller()->RecordGlanceablesBubbleShowTime(
      base::TimeTicks::Now());
  // The calendar view should always keep its size if possible. If there is no
  // enough space, the `time_management_container_view_` should be prioritized
  // to be shrunk. Set the default flex to 0 and manually updates the flex of
  // views depending on the view hierarchy.
  box_layout()->SetDefaultFlex(0);
  box_layout()->set_between_child_spacing(kMarginBetweenGlanceables);
}

GlanceableTrayBubbleView::~GlanceableTrayBubbleView() {
  Shell::Get()->glanceables_controller()->NotifyGlanceablesBubbleClosed();
}

void GlanceableTrayBubbleView::InitializeContents() {
  CHECK(!initialized_);

  // TODO(b/286941809): Apply rounded corners. Temporary removed because they
  // make the background blur to disappear and this requires further
  // investigation.

  const auto* const session_controller = Shell::Get()->session_controller();
  CHECK(session_controller);
  const bool should_show_non_calendar_glanceables =
      session_controller->IsActiveUserSessionStarted() &&
      session_controller->GetSessionState() ==
          session_manager::SessionState::ACTIVE &&
      session_controller->GetUserSession(0)->user_info.has_gaia_account;

  if (!calendar_view_) {
      calendar_container_ =
          AddChildView(std::make_unique<views::FlexLayoutView>());
      calendar_view_ =
          calendar_container_->AddChildView(std::make_unique<CalendarView>(
              /*use_glanceables_container_style=*/true));
      SetCalendarPreferredSize();
  }

  auto* const tasks_client =
      Shell::Get()->glanceables_controller()->GetTasksClient();
  if (should_show_non_calendar_glanceables &&
      features::IsGlanceablesTimeManagementTasksViewEnabled() && tasks_client &&
      !tasks_client->IsDisabledByAdmin()) {
    CHECK(!tasks_bubble_view_);
    auto* cached_list = tasks_client->GetCachedTaskLists();
    if (!cached_list) {
      tasks_client->GetTaskLists(
          /*force_fetch=*/true,
          base::BindOnce(&GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded,
                         weak_ptr_factory_.GetWeakPtr()));
    } else {
      AddTaskBubbleViewIfNeeded(/*fetch_success=*/true, cached_list);
      tasks_client->GetTaskLists(
          /*force_fetch=*/true,
          base::BindOnce(&GlanceableTrayBubbleView::UpdateTaskLists,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }

  const int max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  SetMaxHeight(max_height);
  ChangeAnchorAlignment(shelf_->alignment());
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());

  auto* const classroom_client =
      Shell::Get()->glanceables_controller()->GetClassroomClient();
  const bool is_classroom_enabled_via_flags =
      features::IsGlanceablesTimeManagementClassroomStudentDataEnabled() ||
      features::IsGlanceablesTimeManagementClassroomStudentViewEnabled() ||
      features::AreGlanceablesV2Enabled();
  if (should_show_non_calendar_glanceables && is_classroom_enabled_via_flags &&
      classroom_client) {
    CHECK(!classroom_bubble_student_view_);
    classroom_client->IsStudentRoleActive(base::BindOnce(
        &GlanceableTrayBubbleView::AddClassroomBubbleStudentViewIfNeeded,
        weak_ptr_factory_.GetWeakPtr()));
  }

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
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());
}

void GlanceableTrayBubbleView::OnExpandStateChanged(
    GlanceablesTimeManagementBubbleView::Context context,
    bool is_expanded) {
  // If one of the `GlanceablesTimeManagementBubbleView` is expanded, collapse
  // the other.
  if (context == GlanceablesTimeManagementBubbleView::Context::kClassroom &&
      tasks_bubble_view_) {
    tasks_bubble_view_->SetExpandState(!is_expanded);
    return;
  }

  if (context == GlanceablesTimeManagementBubbleView::Context::kTasks &&
      classroom_bubble_student_view_) {
    classroom_bubble_student_view_->SetExpandState(!is_expanded);
    return;
  }
}

void GlanceableTrayBubbleView::AddClassroomBubbleStudentViewIfNeeded(
    bool is_role_active) {
  if (!is_role_active) {
    return;
  }

  if (features::AreGlanceablesV2Enabled() ||
      features::IsGlanceablesTimeManagementClassroomStudentViewEnabled()) {
    // Adds classroom bubble before `calendar_view_`.
    MaybeCreateTimeManagementContainer();
    classroom_bubble_student_view_ =
        time_management_container_view_->AddChildView(
            std::make_unique<GlanceablesClassroomStudentView>());
    time_management_view_observation_.AddObservation(
        classroom_bubble_student_view_);
    UpdateTimeManagementContainerLayout();
    UpdateBubble();

    AdjustChildrenFocusOrder();
  } else if (features::
                 IsGlanceablesTimeManagementClassroomStudentDataEnabled()) {
    Shell::Get()
        ->glanceables_controller()
        ->GetClassroomClient()
        ->GetStudentAssignmentsWithApproachingDueDate(base::BindOnce(
            &GlanceableTrayBubbleView::OnPotentialStudentAssignmentsLoaded,
            weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded(
    bool fetch_success,
    const ui::ListModel<api::TaskList>* task_lists) {
  if (!fetch_success || task_lists->item_count() == 0) {
    return;
  }

  // Add tasks bubble before everything.
  MaybeCreateTimeManagementContainer();
  tasks_bubble_view_ = time_management_container_view_->AddChildViewAt(
      std::make_unique<GlanceablesTasksView>(task_lists), 0);
  time_management_view_observation_.AddObservation(tasks_bubble_view_);
  UpdateTimeManagementContainerLayout();
  UpdateBubble();

  AdjustChildrenFocusOrder();
}

void GlanceableTrayBubbleView::UpdateTaskLists(
    bool fetch_success,
    const ui::ListModel<api::TaskList>* task_lists) {
  if (fetch_success &&
      features::IsGlanceablesTimeManagementTasksViewEnabled()) {
    views::AsViewClass<GlanceablesTasksView>(tasks_bubble_view_)
        ->UpdateTaskLists(task_lists);
  }
}

void GlanceableTrayBubbleView::AdjustChildrenFocusOrder() {
  auto* default_focused_child = GetChildrenFocusList().front().get();

  // Make sure the view that contains calendar is the first in the focus list of
  // glanceable views.
  if (default_focused_child != calendar_container_) {
    calendar_container_->InsertBeforeInFocusList(default_focused_child);
  }
}

void GlanceableTrayBubbleView::SetCalendarPreferredSize() const {
  // TODO(b/312320532): Update the height if display height is less than
  // `kCalendarBubbleHeightSmallDisplay`.
  calendar_view_->SetPreferredSize(gfx::Size(
      kWideTrayMenuWidth, CalculateMaxTrayBubbleHeight(shelf_->GetWindow()) >
                                  kDisplayHeightThreshold
                              ? kCalendarBubbleHeightLargeDisplay
                              : kCalendarBubbleHeightSmallDisplay));
}

void GlanceableTrayBubbleView::MaybeCreateTimeManagementContainer() {
  if (!time_management_container_view_) {
    time_management_container_view_ =
        AddChildViewAt(std::make_unique<TimeManagementContainer>(), 0);
    box_layout()->SetFlexForView(time_management_container_view_, 1);
  }
}

void GlanceableTrayBubbleView::OnPotentialStudentAssignmentsLoaded(
    bool success,
    std::vector<std::unique_ptr<GlanceablesClassroomAssignment>> assignments)
    const {
  auto* const controller = Shell::Get()->glanceables_controller();
  RecordClassromInitialLoadTime(
      /*first_occurrence=*/controller->bubble_shown_count() == 1,
      base::TimeTicks::Now() - controller->last_bubble_show_time());
}

void GlanceableTrayBubbleView::UpdateTimeManagementContainerLayout() {
  if (time_management_container_view_->children().size() > 1) {
    tasks_bubble_view_->CreateElevatedBackground();
    classroom_bubble_student_view_->CreateElevatedBackground();
  }
}

BEGIN_METADATA(GlanceableTrayBubbleView)
END_METADATA

}  // namespace ash
