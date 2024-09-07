// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/glanceable_tray_bubble_view.h"

#include <memory>
#include <numeric>

#include "ash/api/tasks/tasks_client.h"
#include "ash/api/tasks/tasks_types.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/glanceables/classroom/glanceables_classroom_client.h"
#include "ash/glanceables/classroom/glanceables_classroom_student_view.h"
#include "ash/glanceables/common/glanceables_time_management_bubble_view.h"
#include "ash/glanceables/glanceables_controller.h"
#include "ash/glanceables/tasks/glanceables_tasks_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/session/session_controller_impl.h"
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
#include "base/types/cxx23_to_underlying.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/list_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/layout_types.h"

namespace ash {

using BoundsType = CalendarView::CalendarSlidingSurfaceBoundsType;
using GlanceablesContext = GlanceablesTimeManagementBubbleView::Context;

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

void SetLastExpandedGlanceables(GlanceablesContext context) {
  Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
      prefs::kGlanceablesTimeManagementLastExpandedBubble,
      base::to_underlying(context));
}

GlanceablesContext GetLastExpandedGlanceables() {
  return static_cast<GlanceablesContext>(
      Shell::Get()->session_controller()->GetActivePrefService()->GetInteger(
          prefs::kGlanceablesTimeManagementLastExpandedBubble));
}

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

  views::SizeBounds GetAvailableSize(const View* child) const override {
    // Only consider setting a bounded available size for
    // `GlanceablesTimeManagementBubbleView` children.
    auto* time_management_child =
        views::AsViewClass<GlanceablesTimeManagementBubbleView>(child);
    if (!time_management_child || !time_management_child->IsExpanded()) {
      return views::SizeBounds();
    }

    const auto container_available_height =
        parent()->GetAvailableSize(this).height();
    if (!container_available_height.is_bounded()) {
      return views::SizeBounds();
    }

    int available_height =
        container_available_height.value() - GetInteriorMargin().height();
    bool is_first_visible_child = true;
    for (auto child_iter : children()) {
      if (!child_iter->GetVisible()) {
        continue;
      }
      auto* typed_child =
          views::AsViewClass<GlanceablesTimeManagementBubbleView>(child_iter);
      if (!typed_child) {
        continue;
      }
      if (!is_first_visible_child) {
        available_height -= 8;
      }
      // Assume that only one GlanceablesTimeManagementBubbleView is expanded.
      if (child_iter != time_management_child) {
        available_height -= typed_child->GetCollapsedStatePreferredHeight();
      }
      is_first_visible_child = false;
    }

    views::SizeBounds available_size;
    available_size.set_height(available_height);
    return available_size;
  }

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

// static
void GlanceableTrayBubbleView::RegisterUserProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kGlanceablesTimeManagementLastExpandedBubble,
      base::to_underlying(GlanceablesContext::kTasks));
}

// static
void GlanceableTrayBubbleView::ClearUserStatePrefs(PrefService* prefs) {
  prefs->ClearPref(prefs::kGlanceablesTimeManagementLastExpandedBubble);
}

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
      AddTaskBubbleViewIfNeeded(/*fetch_success=*/true,
                                google_apis::ApiErrorCode::HTTP_SUCCESS,
                                cached_list);
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
  if (should_show_non_calendar_glanceables &&
      features::IsGlanceablesTimeManagementClassroomStudentViewEnabled() &&
      classroom_client && !classroom_client->IsDisabledByAdmin()) {
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

gfx::Size GlanceableTrayBubbleView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int width = TrayBubbleView::CalculatePreferredSize(available_size).width();
  // Let the layout manager calculate the preferred height instead of using the
  // one from TrayBubbleView, which doesn't take the layout manager and margin
  // settings into consider.
  return gfx::Size(
      width,
      std::min(GetLayoutManager()->GetPreferredHeightForWidth(this, width),
               CalculateMaxTrayBubbleHeight(shelf_->GetWindow())));
}

views::SizeBounds GlanceableTrayBubbleView::GetAvailableSize(
    const View* child) const {
  if (child != time_management_container_view_) {
    return TrayBubbleView::GetAvailableSize(child);
  }

  views::SizeBounds available_size;
  auto max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  available_size.set_height(max_height -
                            calendar_view_->GetPreferredSize().height() -
                            kMarginBetweenGlanceables);
  return available_size;
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

void GlanceableTrayBubbleView::OnDidApplyDisplayChanges() {
  int max_height = CalculateMaxTrayBubbleHeight(shelf_->GetWindow());
  SetMaxHeight(max_height);
  SetCalendarPreferredSize();
  ChangeAnchorRect(shelf_->GetSystemTrayAnchorRect());
}

void GlanceableTrayBubbleView::OnExpandStateChanged(GlanceablesContext context,
                                                    bool is_expanded,
                                                    bool expand_by_overscroll) {
  // If one of the `GlanceablesTimeManagementBubbleView` is expanded, collapse
  // the other.
  if (context == GlanceablesContext::kClassroom && tasks_bubble_view_) {
    tasks_bubble_view_->SetExpandState(!is_expanded, expand_by_overscroll);
    SetLastExpandedGlanceables(is_expanded ? GlanceablesContext::kClassroom
                                           : GlanceablesContext::kTasks);
    return;
  }

  if (context == GlanceablesContext::kTasks && classroom_bubble_student_view_) {
    classroom_bubble_student_view_->SetExpandState(!is_expanded,
                                                   expand_by_overscroll);
    SetLastExpandedGlanceables(is_expanded ? GlanceablesContext::kTasks
                                           : GlanceablesContext::kClassroom);
    return;
  }
}

void GlanceableTrayBubbleView::AddClassroomBubbleStudentViewIfNeeded(
    bool is_role_active) {
  if (!is_role_active) {
    return;
  }

  // Adds classroom bubble before `calendar_view_`.
  MaybeCreateTimeManagementContainer();
  classroom_bubble_student_view_ =
      time_management_container_view_->AddChildView(
          std::make_unique<GlanceablesClassroomStudentView>());
  time_management_view_observation_.AddObservation(
      classroom_bubble_student_view_);
  // If `tasks_bubble_view_` exists, collapse either `tasks_bubble_view_` or
  // `classroom_bubble_student_view_` according to the prefs.
  if (tasks_bubble_view_) {
    UpdateChildBubblesInitialExpandState();
  }

  UpdateTimeManagementContainerLayout();
  UpdateBubble();

  AdjustChildrenFocusOrder();
}

void GlanceableTrayBubbleView::AddTaskBubbleViewIfNeeded(
    bool fetch_success,
    std::optional<google_apis::ApiErrorCode> http_error,
    const ui::ListModel<api::TaskList>* task_lists) {
  if (!fetch_success || task_lists->item_count() == 0) {
    return;
  }

  // Add tasks bubble before everything.
  MaybeCreateTimeManagementContainer();
  tasks_bubble_view_ = time_management_container_view_->AddChildViewAt(
      std::make_unique<GlanceablesTasksView>(task_lists), 0);
  time_management_view_observation_.AddObservation(tasks_bubble_view_);
  // If `classroom_bubble_student_view_` exists, collapse either
  // `tasks_bubble_view_` or `classroom_bubble_student_view_` according to the
  // prefs.
  if (classroom_bubble_student_view_) {
    UpdateChildBubblesInitialExpandState();
  }

  UpdateTimeManagementContainerLayout();
  UpdateBubble();

  AdjustChildrenFocusOrder();
}

void GlanceableTrayBubbleView::UpdateChildBubblesInitialExpandState() {
  // By default all children is in expanded states. Directly collapse the one
  // that should be collapsed. Also we only have to update the expand state of
  // one child bubble as `OnExpandStateChanged()` will automatically updates the
  // others.
  if (GetLastExpandedGlanceables() == GlanceablesContext::kTasks) {
    classroom_bubble_student_view_->SetExpandState(
        false, /*expand_by_overscroll=*/false);
  } else {
    tasks_bubble_view_->SetExpandState(false, /*expand_by_overscroll=*/false);
  }
}

void GlanceableTrayBubbleView::UpdateTaskLists(
    bool fetch_success,
    std::optional<google_apis::ApiErrorCode> http_error,
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

void GlanceableTrayBubbleView::UpdateTimeManagementContainerLayout() {
  if (time_management_container_view_->children().size() > 1) {
    tasks_bubble_view_->CreateElevatedBackground();
    classroom_bubble_student_view_->CreateElevatedBackground();
  }
}

BEGIN_METADATA(GlanceableTrayBubbleView)
END_METADATA

}  // namespace ash
