// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_bar_view.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_mini_view_animations.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/stl_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/events/event_observer.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace ash {

namespace {

constexpr int kBarHeightInCompactLayout = 64;
constexpr int kUseCompactLayoutWidthThreshold = 600;

// In the non-compact layout, this is the height allocated for elements other
// than the desk preview (e.g. the DeskNameView, and the vertical paddings).
constexpr int kNonPreviewAllocatedHeight = 55;

// The local Y coordinate of the mini views in both non-compact and compact
// layouts respectively.
constexpr int kMiniViewsY = 16;
constexpr int kMiniViewsYCompact = 8;

// New desk button layout constants.
constexpr int kButtonRightMargin = 36;
constexpr int kIconAndTextHorizontalPadding = 16;
constexpr int kIconAndTextVerticalPadding = 8;

// Spacing between mini views.
constexpr int kMiniViewsSpacing = 12;

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
// DesksBarLayout:

// TODO(minch): Remove this layout manager once the kBento feature is fully
// launched and becomes the default.
// Layout manager for the classic desks bar.
class DesksBarLayout : public views::LayoutManager {
 public:
  DesksBarLayout(views::View* background_view, NewDeskButton* new_desk_button)
      : background_view_(background_view), new_desk_button_(new_desk_button) {}
  DesksBarLayout(const DesksBarLayout&) = delete;
  DesksBarLayout& operator=(const DesksBarLayout&) = delete;
  ~DesksBarLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    auto* desks_bar_view = static_cast<DesksBarView*>(host);
    const bool compact = desks_bar_view->UsesCompactLayout();
    const gfx::Rect bounds = desks_bar_view->bounds();
    background_view_->SetBoundsRect(bounds);

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
        bounds.right() - new_desk_button_size.width() - kButtonRightMargin,
        (bounds.height() - new_desk_button_size.height()) / 2,
        new_desk_button_size.width(), new_desk_button_size.height()};
    new_desk_button_->SetBoundsRect(button_bounds);

    const std::vector<DeskMiniView*>& mini_views = desks_bar_view->mini_views();
    if (mini_views.empty())
      return;

    const gfx::Size mini_view_size = mini_views[0]->GetPreferredSize();
    const int total_width =
        mini_views.size() * (mini_view_size.width() + kMiniViewsSpacing) -
        kMiniViewsSpacing;

    int x = (bounds.width() - total_width) / 2;
    const int y = compact ? kMiniViewsYCompact : kMiniViewsY;
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(gfx::Rect(gfx::Point(x, y), mini_view_size));
      x += (mini_view_size.width() + kMiniViewsSpacing);
    }
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    DCHECK(host);
    return host->bounds().size();
  }

 private:
  views::View* background_view_;    // Not owned.
  NewDeskButton* new_desk_button_;  // Not owned.
};

// -----------------------------------------------------------------------------
// BentoDesksBarLayout:

// TODO(minch): Remove this layout manager and move the layout code back to
// DesksBarView::Layout() once the kBento feature is launched and becomes
// stable.
// Layout manager for desks bar of Bento. The difference from DesksBarLayout is
// that there is no compact layout in Bento. And contents can be layout outside
// of the bar if the total contents' width exceeds the width of the desks bar.
class BentoDesksBarLayout : public views::LayoutManager {
 public:
  BentoDesksBarLayout(DesksBarView* desks_bar_view,
                      NewDeskButton* new_desk_button)
      : desks_bar_view_(desks_bar_view), new_desk_button_(new_desk_button) {}
  BentoDesksBarLayout(const BentoDesksBarLayout&) = delete;
  BentoDesksBarLayout& operator=(const BentoDesksBarLayout&) = delete;
  ~BentoDesksBarLayout() override = default;

  // views::LayoutManager:
  void Layout(views::View* host) override {
    const gfx::Rect desks_bar_bounds = desks_bar_view_->bounds();
    gfx::Size new_desk_button_size = new_desk_button_->GetPreferredSize();
    new_desk_button_size.Enlarge(2 * kIconAndTextHorizontalPadding,
                                 2 * kIconAndTextVerticalPadding);

    const std::vector<DeskMiniView*>& mini_views =
        desks_bar_view_->mini_views();
    int content_width = new_desk_button_size.width() + 2 * kMiniViewsSpacing;
    gfx::Size mini_view_size;
    if (!mini_views.empty()) {
      mini_view_size = mini_views[0]->GetPreferredSize();
      content_width +=
          mini_views.size() * (mini_view_size.width() + kMiniViewsSpacing);
    }

    width_ = std::max(desks_bar_bounds.width(), content_width);

    // Update the size of the |host|, which is |scroll_view_contents_| here.
    // This is done to make sure its size can be updated on mini views' adding
    // or removing, then |scroll_view_| will know whether the contents need to
    // be scolled or not.
    host->SetSize(gfx::Size(width_, desks_bar_bounds.height()));

    const gfx::Rect button_bounds(
        width_ - new_desk_button_size.width() - kMiniViewsSpacing,
        (desks_bar_bounds.height() - new_desk_button_size.height()) / 2,
        new_desk_button_size.width(), new_desk_button_size.height());
    new_desk_button_->SetBoundsRect(button_bounds);

    if (mini_views.empty())
      return;

    const int width_for_mini_views =
        width_ - kMiniViewsSpacing - new_desk_button_size.width();
    const int mini_views_width =
        mini_views.size() * (mini_view_size.width() + kMiniViewsSpacing) -
        kMiniViewsSpacing;
    int x = (width_for_mini_views - mini_views_width) / 2;
    for (auto* mini_view : mini_views) {
      mini_view->SetBoundsRect(
          gfx::Rect(gfx::Point(x, kMiniViewsY), mini_view_size));
      x += (mini_view_size.width() + kMiniViewsSpacing);
    }
  }

  // views::LayoutManager:
  gfx::Size GetPreferredSize(const views::View* host) const override {
    return gfx::Size(width_, desks_bar_view_->bounds().height());
  }

 private:
  DesksBarView* desks_bar_view_;    // Not owned.
  NewDeskButton* new_desk_button_;  // Not owned.

  // Width of the scroll view. It is the contents' preferred width if it exceeds
  // the desks bar view's width or just the desks bar view's width if not.
  int width_ = 0;
};

// -----------------------------------------------------------------------------
// DesksBarView:

DesksBarView::DesksBarView(OverviewGrid* overview_grid)
    : background_view_(new views::View),
      new_desk_button_(new NewDeskButton()),
      overview_grid_(overview_grid) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  background_view_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  background_view_->layer()->SetFillsBoundsOpaquely(false);

  AddChildView(background_view_);

  if (features::IsBentoEnabled()) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    scroll_view_ = AddChildView(std::make_unique<views::ScrollView>());
    scroll_view_->SetBackgroundColor(base::nullopt);
    scroll_view_->SetDrawOverflowIndicator(false);
    scroll_view_->SetHorizontalScrollBarMode(
        views::ScrollView::ScrollBarMode::kHiddenButEnabled);
    scroll_view_->SetTreatAllScrollEventsAsHorizontal(true);

    scroll_view_contents_ =
        scroll_view_->SetContents(std::make_unique<views::View>());
    scroll_view_contents_->AddChildView(new_desk_button_);
    scroll_view_contents_->SetLayoutManager(
        std::make_unique<BentoDesksBarLayout>(this, new_desk_button_));
  } else {
    AddChildView(new_desk_button_);
    SetLayoutManager(
        std::make_unique<DesksBarLayout>(background_view_, new_desk_button_));
  }

  DesksController::Get()->AddObserver(this);
}

DesksBarView::~DesksBarView() {
  DesksController::Get()->RemoveObserver(this);
}

// static
int DesksBarView::GetBarHeightForWidth(aura::Window* root,
                                       const DesksBarView* desks_bar_view,
                                       int width) {
  if (!features::IsBentoEnabled() &&
      (width <= kUseCompactLayoutWidthThreshold ||
       (desks_bar_view &&
        width <= desks_bar_view->min_width_to_fit_contents_))) {
    return kBarHeightInCompactLayout;
  }

  return DeskPreviewView::GetHeight(root, /*compact=*/false) +
         kNonPreviewAllocatedHeight;
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
  params.activatable = views::Widget::InitParams::ACTIVATABLE_YES;
  params.accept_events = true;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  // This widget will be parented to the currently-active desk container on
  // |root|.
  params.context = root;
  params.bounds = bounds;
  params.name = "VirtualDesksWidget";

  // Even though this widget exists on the active desk container, it should not
  // show up in the MRU list, and it should not be mirrored in the desks
  // mini_views.
  params.init_properties_container.SetProperty(kExcludeInMruKey, true);
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  widget->Init(std::move(params));

  auto* window = widget->GetNativeWindow();
  window->set_id(kShellWindowId_DesksBarWindow);
  ::wm::SetWindowVisibilityAnimationTransition(window, ::wm::ANIMATE_NONE);

  return widget;
}

void DesksBarView::Init() {
  UpdateNewMiniViews(/*initializing_bar_view=*/true);
  hover_observer_ = std::make_unique<DeskBarHoverObserver>(
      this, GetWidget()->GetNativeWindow());
}

bool DesksBarView::IsDeskNameBeingModified() const {
  if (!GetWidget()->IsActive())
    return false;

  for (auto* mini_view : mini_views_) {
    if (mini_view->IsDeskNameBeingModified())
      return true;
  }
  return false;
}

float DesksBarView::GetOnHoverWindowSizeScaleFactor() const {
  return float{height()} / overview_grid_->root_window()->bounds().height();
}

void DesksBarView::OnHoverStateMayHaveChanged() {
  for (auto* mini_view : mini_views_)
    mini_view->UpdateCloseButtonVisibility();
}

void DesksBarView::OnGestureTap(const gfx::Rect& screen_rect,
                                bool is_long_gesture) {
  for (auto* mini_view : mini_views_)
    mini_view->OnWidgetGestureTap(screen_rect, is_long_gesture);
}

void DesksBarView::SetDragDetails(const gfx::Point& screen_location,
                                  bool dragged_item_over_bar) {
  last_dragged_item_screen_location_ = screen_location;
  const bool old_dragged_item_over_bar = dragged_item_over_bar_;
  dragged_item_over_bar_ = dragged_item_over_bar;

  if (!old_dragged_item_over_bar && !dragged_item_over_bar)
    return;

  for (auto* mini_view : mini_views_)
    mini_view->UpdateBorderColor();
}

const char* DesksBarView::GetClassName() const {
  return "DesksBarView";
}

bool DesksBarView::OnMousePressed(const ui::MouseEvent& event) {
  DeskNameView::CommitChanges(GetWidget());
  return false;
}

void DesksBarView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_DOWN:
      DeskNameView::CommitChanges(GetWidget());
      break;

    default:
      break;
  }
}

void DesksBarView::OnThemeChanged() {
  views::View::OnThemeChanged();
  DCHECK_EQ(ui::LAYER_SOLID_COLOR, background_view_->layer()->type());
  background_view_->layer()->SetColor(
      AshColorProvider::Get()->GetShieldLayerColor(
          AshColorProvider::ShieldLayerType::kShield80));
}

bool DesksBarView::UsesCompactLayout() const {
  if (features::IsBentoEnabled())
    return false;

  return width() <= kUseCompactLayoutWidthThreshold ||
         width() <= min_width_to_fit_contents_;
}

void DesksBarView::OnDeskAdded(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  UpdateNewMiniViews(/*initializing_bar_view=*/false);
}

void DesksBarView::OnDeskRemoved(const Desk* desk) {
  DeskNameView::CommitChanges(GetWidget());
  auto iter = std::find_if(
      mini_views_.begin(), mini_views_.end(),
      [desk](DeskMiniView* mini_view) { return desk == mini_view->desk(); });

  DCHECK(iter != mini_views_.end());

  // Let the highlight controller know the view is destroying before it is
  // removed from the collection because it needs to know the index of the mini
  // view, or the desk name view (if either is currently highlighted) relative
  // to other traversable views.
  auto* highlight_controller = GetHighlightController();
  // The order here matters, we call it first on the desk_name_view since it
  // comes later in the highlight order (See documentation of
  // OnViewDestroyingOrDisabling()).
  highlight_controller->OnViewDestroyingOrDisabling((*iter)->desk_name_view());
  highlight_controller->OnViewDestroyingOrDisabling(*iter);

  const int begin_x = GetFirstMiniViewXOffset();
  // Remove the mini view from the list now. And remove it from its parent
  // after the animation is done.
  DeskMiniView* removed_mini_view = *iter;
  auto partition_iter = mini_views_.erase(iter);

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();
  new_desk_button_->UpdateButtonState();

  for (auto* mini_view : mini_views_)
    mini_view->UpdateCloseButtonVisibility();

  PerformRemoveDeskMiniViewAnimation(
      removed_mini_view,
      std::vector<DeskMiniView*>(mini_views_.begin(), partition_iter),
      std::vector<DeskMiniView*>(partition_iter, mini_views_.end()),
      begin_x - GetFirstMiniViewXOffset());
}

void DesksBarView::OnDeskActivationChanged(const Desk* activated,
                                           const Desk* deactivated) {
  for (auto* mini_view : mini_views_) {
    const Desk* desk = mini_view->desk();
    if (desk == activated || desk == deactivated)
      mini_view->UpdateBorderColor();
  }
}

void DesksBarView::OnDeskSwitchAnimationLaunching() {}

void DesksBarView::OnDeskSwitchAnimationFinished() {}

void DesksBarView::UpdateNewMiniViews(bool initializing_bar_view) {
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
      DeskMiniView* mini_view = AddMiniViewAsChild(
          std::make_unique<DeskMiniView>(this, root_window, desk.get()));
      mini_views_.push_back(mini_view);
      new_mini_views.push_back(mini_view);
    }
  }

  if (features::IsBentoEnabled() && !initializing_bar_view) {
    // If Bento is enabled, focus on the newly created name view to encourage
    // users to rename their desks.
    auto* newly_added_name_view = mini_views_.back()->desk_name_view();
    newly_added_name_view->RequestFocus();

    // Set |newly_added_name_view|'s accessible name to the default desk name
    // since its text is cleared.
    newly_added_name_view->SetAccessibleName(
        DesksController::GetDeskDefaultName(desks.size() - 1));

    auto* highlight_controller = GetHighlightController();
    if (highlight_controller->IsFocusHighlightVisible())
      highlight_controller->MoveHighlightToView(newly_added_name_view);
  }

  UpdateMinimumWidthToFitContents();
  overview_grid_->OnDesksChanged();

  if (initializing_bar_view)
    return;

  PerformNewDeskMiniViewAnimation(this, new_mini_views,
                                  begin_x - GetFirstMiniViewXOffset(),
                                  first_time_mini_views);
}

DeskMiniView* DesksBarView::FindMiniViewForDesk(const Desk* desk) const {
  for (auto* mini_view : mini_views_) {
    if (mini_view->desk() == desk)
      return mini_view;
  }

  return nullptr;
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

DeskMiniView* DesksBarView::AddMiniViewAsChild(
    std::unique_ptr<DeskMiniView> mini_view) {
  return features::IsBentoEnabled()
             ? scroll_view_contents_->AddChildView(std::move(mini_view))
             : AddChildView(std::move(mini_view));
}

}  // namespace ash
