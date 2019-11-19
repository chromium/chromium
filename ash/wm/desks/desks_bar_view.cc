// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_observer.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr int kBarHeight = 104;
constexpr int kBarHeightInCompactLayout = 64;
constexpr int kUseCompactLayoutWidthThreshold = 600;

// New desk button layout constants.
constexpr int kButtonRightMargin = 36;
constexpr int kIconAndTextHorizontalPadding = 16;
constexpr int kIconAndTextVerticalPadding = 8;

// Spacing between mini views.
constexpr int kMiniViewsSpacing = 12;

base::string16 GetMiniViewTitle(int mini_view_index) {
  DCHECK_GE(mini_view_index, 0);
  DCHECK_LT(mini_view_index, 4);
  constexpr int kStringIds[] = {IDS_ASH_DESKS_DESK_1_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_2_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_3_MINI_VIEW_TITLE,
                                IDS_ASH_DESKS_DESK_4_MINI_VIEW_TITLE};

  return l10n_util::GetStringUTF16(kStringIds[mini_view_index]);
}

gfx::Rect GetGestureEventScreenRect(const ui::Event& event) {
  DCHECK(event.IsGestureEvent());
  return event.AsGestureEvent()->details().bounding_box();
}

OverviewHighlightController* GetHighlightController() {
  auto* overview_controller = Shell::Get()->overview_controller();
  DCHECK(overview_controller->InOverviewSession());
  return overview_controller->overview_session()->highlight_controller();
}

}  // namespace

// -----------------------------------------------------------------------------
// DeskBarHoverObserver:

class DeskBarHoverObserver : public ui::EventObserver {
 public:
  DeskBarHoverObserver(DesksBarView* owner, aura::Window* widget_window)
      : owner_(owner),
        event_monitor_(views::EventMonitor::CreateWindowMonitor(
            this,
            widget_window,
            {ui::ET_MOUSE_PRESSED, ui::ET_MOUSE_DRAGGED, ui::ET_MOUSE_RELEASED,
             ui::ET_MOUSE_MOVED, ui::ET_MOUSE_ENTERED, ui::ET_MOUSE_EXITED,
             ui::ET_GESTURE_LONG_PRESS, ui::ET_GESTURE_LONG_TAP,
             ui::ET_GESTURE_TAP, ui::ET_GESTURE_TAP_DOWN})) {}

  ~DeskBarHoverObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    switch (event.type()) {
      case ui::ET_MOUSE_PRESSED:
      case ui::ET_MOUSE_DRAGGED:
      case ui::ET_MOUSE_RELEASED:
      case ui::ET_MOUSE_MOVED:
      case ui::ET_MOUSE_ENTERED:
      case ui::ET_MOUSE_EXITED:
        owner_->OnHoverStateMayHaveChanged();
        break;

      case ui::ET_GESTURE_LONG_PRESS:
      case ui::ET_GESTURE_LONG_TAP:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/true);
        break;

      case ui::ET_GESTURE_TAP:
      case ui::ET_GESTURE_TAP_DOWN:
        owner_->OnGestureTap(GetGestureEventScreenRect(event),
                             /*is_long_gesture=*/false);
        break;

      default:
        NOTREACHED();
        break;
    }
  }

 private:
  DesksBarView* owner_;

  std::unique_ptr<views::EventMonitor> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(DeskBarHoverObserver);
};

// -----------------------------------------------------------------------------
// DesksBarView:

DesksBarView::DesksBarView(OverviewGrid* overview_grid)
    : background_view_(new views::View),
      new_desk_button_(new NewDeskButton(this)),
      overview_grid_(overview_grid) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  background_view_->layer()->SetFillsBoundsOpaquely(false);
  background_view_->layer()->SetColor(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparentWithBlur,
          AshColorProvider::AshColorMode::kDark));

  AddChildView(background_view_);
  AddChildView(new_desk_button_);

  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
}

// static
int DesksBarView::GetBarHeightForWidth(const DesksBarView* desks_bar_view,
                                       int width) {
  if (width <= kUseCompactLayoutWidthThreshold ||
      (desks_bar_view && width <= desks_bar_view->min_width_to_fit_contents_)) {
    return kBarHeightInCompactLayout;
  }

  return kBarHeight;
}

// static
std::unique_ptr<views::Widget> DesksBarView::CreateDesksWidget(
    aura::Window* root,
    const gfx::Rect& bounds) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto widget = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // Use the wallpaper container similar to all background widgets created in
  // overview mode.
  params.parent = root->GetChildById(kShellWindowId_WallpaperContainer);
  params.bounds = bounds;
  params.name = "VirtualDesksWidget";
  widget->Init(std::move(params));
  ::wm::SetWindowVisibilityAnimationTransition(widget->GetNativeWindow(),
                                               ::wm::ANIMATE_NONE);

  return widget;
}

void DesksBarView::Init() {
  UpdateNewMiniViews(/*animate=*/false);
  hover_observer_ = std::make_unique<DeskBarHoverObserver>(
      this, GetWidget()->GetNativeWindow());
}

void DesksBarView::OnHoverStateMayHaveChanged() {
  for (auto& mini_view : mini_views_)
    mini_view->OnHoverStateMayHaveChanged();
}

void DesksBarView::OnGestureTap(const gfx::Rect& screen_rect,
                                bool is_long_gesture) {
  for (auto& mini_view : mini_views_)
    mini_view->OnWidgetGestureTap(screen_rect, is_long_gesture);
}

void DesksBarView::SetDragDetails(const gfx::Point& screen_location,
                                  bool dragged_item_over_bar) {
  last_dragged_item_screen_location_ = screen_location;
  const bool old_dragged_item_over_bar = dragged_item_over_bar_;
  dragged_item_over_bar_ = dragged_item_over_bar;

  if (!old_dragged_item_over_bar && !dragged_item_over_bar)
    return;

  for (auto& mini_view : mini_views_)
    mini_view->UpdateBorderColor();
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
}

void DesksBarView::Layout() {
  background_view_->SetBoundsRect(bounds());

  const bool compact = UsesCompactLayout();
  new_desk_button_->SetLabelVisible(!compact);
  gfx::Size new_desk_button_size = new_desk_button_->GetPreferredSize();
  if (compact) {
    new_desk_button_size.Enlarge(2 * kIconAndTextVerticalPadding,
                                 2 * kIconAndTextVerticalPadding);
  } else {
    new_desk_button_size.Enlarge(2 * kIconAndTextHorizontalPadding,
                                 2 * kIconAndTextVerticalPadding);
  }

  const gfx::Rect button_bounds{
      bounds().right() - new_desk_button_size.width() - kButtonRightMargin,
      (bounds().height() - new_desk_button_size.height()) / 2,
      new_desk_button_size.width(), new_desk_button_size.height()};
  new_desk_button_->SetBoundsRect(button_bounds);

  if (mini_views_.empty())
    return;

  const gfx::Size mini_view_size = mini_views_[0]->GetPreferredSize();
  const int total_width =
      mini_views_.size() * (mini_view_size.width() + kMiniViewsSpacing) -
      kMiniViewsSpacing;
  gfx::Rect mini_views_bounds = bounds();
  mini_views_bounds.ClampToCenteredSize(
      gfx::Size(total_width, mini_view_size.height()));

  int x = mini_views_bounds.x();
  const int y = mini_views_bounds.y();
  for (auto& mini_view : mini_views_) {
    mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
    x += (mini_view_size.width() + kMiniViewsSpacing);
  }
}

bool DesksBarView::UsesCompactLayout() const {
  return width() <= kUseCompactLayoutWidthThreshold ||
         width() <= min_width_to_fit_contents_;
}

void DesksBarView::ButtonPressed(views::Button* sender,
                                 const ui::Event& event) {
  auto* controller = DesksController::Get();
  if (sender == new_desk_button_) {
    new_desk_button_->OnButtonPressed();
    return;
  }

  for (auto& mini_view : mini_views_) {
    if (mini_view.get() == sender) {
      controller->ActivateDesk(mini_view->desk(),
                               DesksSwitchSource::kMiniViewButton);
      return;
    }
  }
}

void DesksBarView::OnDeskAdded(const Desk* desk) {
  UpdateNewMiniViews(/*animate=*/true);
}

void DesksBarView::OnDeskRemoved(const Desk* desk) {
  auto iter =
      std::find_if(mini_views_.begin(), mini_views_.end(),
                   [desk](const std::unique_ptr<DeskMiniView>& mini_view) {
                     return desk == mini_view->desk();
                   });

  DCHECK(iter != mini_views_.end());

  // Let the highlight controller know the view is destroying before it is
  // removed from the collection because it needs to know the index of the mini
  // view relative to other traversable views.
  GetHighlightController()->OnViewDestroyingOrDisabling(iter->get());

  const int begin_x = GetFirstMiniViewXOffset();
  std::unique_ptr<DeskMiniView> removed_mini_view = std::move(*iter);
  auto partition_iter = mini_views_.erase(iter);

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();

  UpdateMiniViewsLabels();
  new_desk_button_->UpdateButtonState();

  std::vector<DeskMiniView*> mini_views_before;
  std::vector<DeskMiniView*> mini_views_after;
  const auto transform_lambda =
      [](const std::unique_ptr<DeskMiniView>& mini_view) {
        return mini_view.get();
      };

  std::transform(mini_views_.begin(), partition_iter,
                 std::back_inserter(mini_views_before), transform_lambda);
  std::transform(partition_iter, mini_views_.end(),
                 std::back_inserter(mini_views_after), transform_lambda);

  PerformRemoveDeskMiniViewAnimation(std::move(removed_mini_view),
                                     mini_views_before, mini_views_after,
                                     begin_x - GetFirstMiniViewXOffset());
}

void DesksBarView::OnDeskActivationChanged(const Desk* activated,
                                           const Desk* deactivated) {
  for (auto& mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated)
      mini_view->UpdateBorderColor();
  }
}

void DesksBarView::OnDeskSwitchAnimationLaunching() {}

void DesksBarView::OnDeskSwitchAnimationFinished() {}

void DesksBarView::UpdateNewMiniViews(bool animate) {
  const auto& desks = DesksController::Get()->desks();
  if (desks.size() < 2) {
    // We do not show mini_views when we have a single desk.
    DCHECK(mini_views_.empty());

    // The bar background is initially translated off the screen.
    gfx::Transform translate;
    translate.Translate(0, -height());
    background_view_->layer()->SetTransform(translate);
    background_view_->layer()->SetOpacity(0);

    return;
  }

  // This should not be called when a desk is removed.
  DCHECK_LE(mini_views_.size(), desks.size());

  const bool first_time_mini_views = mini_views_.empty();
  const int begin_x = GetFirstMiniViewXOffset();
  std::vector<DeskMiniView*> new_mini_views;

  aura::Window* root_window = GetWidget()->GetNativeWindow()->GetRootWindow();
  DCHECK(root_window);
  for (const auto& desk : desks) {
    if (!FindMiniViewForDesk(desk.get())) {
      mini_views_.emplace_back(std::make_unique<DeskMiniView>(
          this, root_window, desk.get(), GetMiniViewTitle(mini_views_.size())));
      DeskMiniView* mini_view = mini_views_.back().get();
      mini_view->set_owned_by_client();
      new_mini_views.emplace_back(mini_view);
      AddChildView(mini_view);
    }
  }

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();

  if (!animate)
    return;

  PerformNewDeskMiniViewAnimation(this, new_mini_views,
                                  begin_x - GetFirstMiniViewXOffset(),
                                  first_time_mini_views);
}

DeskMiniView* DesksBarView::FindMiniViewForDesk(const Desk* desk) const {
  for (auto& mini_view : mini_views_) {
    if (mini_view->desk() == desk)
      return mini_view.get();
  }

  return nullptr;
}

void DesksBarView::UpdateMiniViewsLabels() {
  // TODO(afakhry): Don't do this for user-modified desk labels.
  size_t i = 0;
  for (auto& mini_view : mini_views_)
    mini_view->SetTitle(GetMiniViewTitle(i++));
}

int DesksBarView::GetFirstMiniViewXOffset() const {
  return mini_views_.empty() ? bounds().CenterPoint().x()
                             : mini_views_[0]->bounds().x();
}

void DesksBarView::UpdateMinimumWidthToFitContents() {
  int button_width = new_desk_button_->GetMinSize(/*compact=*/false).width();
  button_width += 2 * kIconAndTextHorizontalPadding;
  button_width += kButtonRightMargin;

  if (mini_views_.empty()) {
    min_width_to_fit_contents_ = button_width;
    return;
  }

  const int mini_view_width = mini_views_[0]->GetMinWidthForDefaultLayout();
  const int total_mini_views_width =
      mini_views_.size() * (mini_view_width + kMiniViewsSpacing) -
      kMiniViewsSpacing;

  min_width_to_fit_contents_ = total_mini_views_width + button_width * 2;
}

}  // namespace ash
