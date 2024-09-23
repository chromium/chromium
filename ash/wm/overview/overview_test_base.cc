// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_test_base.h"

#include <tuple>

#include "ash/public/cpp/test/test_saved_desk_delegate.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/style/close_button.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/style/system_shadow.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

OverviewTestBase::~OverviewTestBase() = default;

void OverviewTestBase::EnterTabletMode() {
  // To avoid flaky failures due to mouse devices blocking entering tablet mode,
  // we detach all mouse devices.
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
  base::RunLoop().RunUntilIdle();
}

void OverviewTestBase::LeaveTabletMode() {
  TabletModeControllerTestApi().LeaveTabletMode();
}

bool OverviewTestBase::InOverviewSession() const {
  return OverviewController::Get()->InOverviewSession();
}

bool OverviewTestBase::WindowsOverlapping(aura::Window* window1,
                                          aura::Window* window2) const {
  const gfx::Rect window1_bounds = GetTransformedTargetBounds(window1);
  const gfx::Rect window2_bounds = GetTransformedTargetBounds(window2);
  return window1_bounds.Intersects(window2_bounds);
}

std::unique_ptr<aura::Window> OverviewTestBase::CreateUnsnappableWindow(
    const gfx::Rect& bounds) {
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorNone);
  return window;
}

void OverviewTestBase::ClickWindow(aura::Window* window) {
  ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
  event_generator.ClickLeftButton();
}

OverviewController* OverviewTestBase::GetOverviewController() {
  return OverviewController::Get();
}

OverviewSession* OverviewTestBase::GetOverviewSession() {
  return GetOverviewController()->overview_session();
}

SplitViewController* OverviewTestBase::GetSplitViewController() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

gfx::Rect OverviewTestBase::GetTransformedBounds(aura::Window* window) const {
  gfx::RectF bounds(window->layer()->bounds());
  wm::TranslateRectToScreen(window->parent(), &bounds);
  const gfx::Transform transform =
      gfx::TransformAboutPivot(bounds.origin(), window->layer()->transform());
  return ToStableSizeRoundedRect(transform.MapRect(bounds));
}

gfx::Rect OverviewTestBase::GetTransformedTargetBounds(
    aura::Window* window) const {
  gfx::RectF bounds(window->layer()->GetTargetBounds());
  wm::TranslateRectToScreen(window->parent(), &bounds);
  const gfx::Transform transform = gfx::TransformAboutPivot(
      bounds.origin(), window->layer()->GetTargetTransform());
  return ToStableSizeRoundedRect(transform.MapRect(bounds));
}

gfx::Rect OverviewTestBase::GetTransformedBoundsInRootWindow(
    aura::Window* window) const {
  aura::Window* root = window->GetRootWindow();
  CHECK(window->layer());
  CHECK(root->layer());
  gfx::Transform transform;
  if (!window->layer()->GetTargetTransformRelativeTo(root->layer(),
                                                     &transform)) {
    return gfx::Rect();
  }
  return transform.MapRect(gfx::Rect(window->bounds().size()));
}

const OverviewItemBase* OverviewTestBase::GetDropTarget(int grid_index) const {
  return OverviewController::Get()
      ->overview_session()
      ->grid_list_[grid_index]
      ->drop_target();
}

CloseButton* OverviewTestBase::GetCloseButton(OverviewItemBase* item) {
  return item->GetLeafItemForWindow(item->GetWindow())
      ->overview_item_view_->close_button();
}

views::Label* OverviewTestBase::GetLabelView(OverviewItemBase* item) {
  return item->GetLeafItemForWindow(item->GetWindow())
      ->overview_item_view_->header_view()
      ->title_label();
}

views::View* OverviewTestBase::GetBackdropView(OverviewItemBase* item) {
  return item->GetBackDropView();
}

WindowPreviewView* OverviewTestBase::GetPreviewView(OverviewItemBase* item) {
  return item->GetLeafItemForWindow(item->GetWindow())
      ->overview_item_view_->preview_view();
}

gfx::Rect OverviewTestBase::GetShadowBounds(
    const OverviewItemBase* item) const {
  SystemShadow* shadow = item->shadow_.get();
  if (!shadow || !shadow->GetLayer()->visible()) {
    return gfx::Rect();
  }

  return shadow->GetContentBounds();
}

views::Widget* OverviewTestBase::GetCannotSnapWidget(OverviewItemBase* item) {
  return item->cannot_snap_widget_.get();
}

void OverviewTestBase::SetAnimatingToClose(OverviewItemBase* item, bool val) {
  item->animating_to_close_ = val;
}

float OverviewTestBase::GetCloseButtonOpacity(OverviewItemBase* item) {
  return GetCloseButton(item)->layer()->opacity();
}

float OverviewTestBase::GetTitlebarOpacity(OverviewItemBase* item) {
  return item->GetLeafItemForWindow(item->GetWindow())
      ->overview_item_view_->header_view()
      ->layer()
      ->opacity();
}

bool OverviewTestBase::HasRoundedCorner(OverviewItemBase* item) {
  aura::Window* window = item->GetWindow();
  const ui::Layer* layer = window_util::IsMinimizedOrTucked(window)
                               ? GetPreviewView(item)->layer()
                               : window->layer();
  return !layer->rounded_corner_radii().IsEmpty();
}

void OverviewTestBase::CheckWindowAndCloseButtonInScreen(
    aura::Window* window,
    OverviewItemBase* window_item) {
  const gfx::Rect screen_bounds =
      window_item->root_window()->GetBoundsInScreen();
  EXPECT_TRUE(window_item->Contains(window));
  EXPECT_TRUE(screen_bounds.Contains(GetTransformedTargetBounds(window)));
  EXPECT_TRUE(
      screen_bounds.Contains(GetCloseButton(window_item)->GetBoundsInScreen()));
}

void OverviewTestBase::SetUp() {
  AshTestBase::SetUp();

  aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
  shelf_view_test_api_ = std::make_unique<ShelfViewTestAPI>(
      GetPrimaryShelf()->GetShelfViewForTesting());
  shelf_view_test_api_->SetAnimationDuration(base::Milliseconds(1));
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(
      /*immediate=*/true);
  ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
      true);
}

void OverviewTestBase::TearDown() {
  ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
      false);
  trace_names_.clear();
  AshTestBase::TearDown();
}

void OverviewTestBase::CheckForDuplicateTraceName(const std::string& trace) {
  DCHECK(!base::Contains(trace_names_, trace)) << trace;
  trace_names_.push_back(trace);
}

void OverviewTestBase::CheckA11yOverrides(const std::string& trace,
                                          views::Widget* widget,
                                          views::Widget* expected_previous,
                                          views::Widget* expected_next) {
  SCOPED_TRACE(trace);
  views::View* contents_view = widget->GetContentsView();
  views::ViewAccessibility& view_accessibility =
      contents_view->GetViewAccessibility();
  EXPECT_EQ(expected_previous, view_accessibility.GetPreviousWindowFocus());
  EXPECT_EQ(expected_next, view_accessibility.GetNextWindowFocus());
}

void OverviewTestBase::CheckOverviewEnterExitHistogram(
    const std::string& trace,
    const std::vector<int>& enter_counts,
    const std::vector<int>& exit_counts) {
  CheckForDuplicateTraceName(trace);

  // Force frames and wait for all throughput trackers to be gone to allow
  // animation throughput data to be passed from cc to ui.
  ui::Compositor* compositor =
      Shell::GetPrimaryRootWindow()->layer()->GetCompositor();
  while (compositor->has_throughput_trackers_for_testing()) {
    compositor->ScheduleFullRedraw();
    std::ignore =
        ui::WaitForNextFrameToBePresented(compositor, base::Milliseconds(500));
  }

  {
    SCOPED_TRACE(trace + ".Enter");
    CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Enter",
                           enter_counts);
  }
  {
    SCOPED_TRACE(trace + ".Exit");
    CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Exit",
                           exit_counts);
  }
}

gfx::Rect OverviewTestBase::GetGridBounds() {
  if (GetOverviewSession())
    return GetOverviewSession()->grid_list_[0]->bounds_;

  return gfx::Rect();
}

void OverviewTestBase::SetGridBounds(OverviewGrid* grid,
                                     const gfx::Rect& bounds) {
  grid->bounds_ = bounds;
}

void OverviewTestBase::CheckOverviewHistogram(const std::string& histogram,
                                              const std::vector<int>& counts) {
  ASSERT_EQ(5u, counts.size());

  histograms_.ExpectTotalCount(histogram + ".ClamshellMode", counts[0]);
  histograms_.ExpectTotalCount(histogram + ".SingleClamshellMode", counts[1]);
  histograms_.ExpectTotalCount(histogram + ".TabletMode", counts[2]);
  histograms_.ExpectTotalCount(histogram + ".MinimizedTabletMode", counts[3]);
  histograms_.ExpectTotalCount(histogram + ".SplitView", counts[4]);
}
}  // namespace ash
