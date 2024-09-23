// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_library_view.h"

#include "ash/controls/rounded_scroll_bar.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_label.h"
#include "ash/style/typography.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/templates/saved_desk_constants.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/window_properties.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/layout/box_layout.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Grids use landscape mode if the available width is greater or equal to this.
constexpr int kLandscapeMinWidth = 756;

// "No items" label dimensions.
constexpr gfx::Size kNoItemsLabelPadding = {16, 8};
constexpr int kNoItemsLabelHeight = 32;

// Between child spacing of Library page scroll content view.
constexpr int kLibraryPageScrollContentsBetweenChildSpacingDp = 32;

// Between child spacing of group content view.
constexpr int kGroupContentsBetweenChildSpacingDp = 20;

// The size of the gradient applied to the top and bottom of the scroll view.
constexpr int kScrollViewGradientSize = 32;

// Insets of Library page scroll content view. Note: the bottom inset is there
// to slightly adjust the otherwise vertically centered scroll content up a tad.
constexpr gfx::Insets kLibraryPageScrollContentsInsets =
    gfx::Insets::TLBR(32, 0, 96, 0);

// Insets for the vertical scroll bar.
constexpr gfx::Insets kLibraryPageVerticalScrollInsets =
    gfx::Insets::TLBR(1, 0, 1, 1);

// The animation duration for the desk item to move up into the desk bar.
constexpr base::TimeDelta kSaveAndRecallLaunchMoveDuration =
    base::Milliseconds(300);

// The delay before the desk item crossfades into the desk preview happens.
constexpr base::TimeDelta kSaveAndRecallLaunchFadeDelay =
    base::Milliseconds(250);

// The duration of the above crossfade.
constexpr base::TimeDelta kSaveAndRecallLaunchFadeDuration =
    base::Milliseconds(250);

struct SavedDesks {
  // Saved desks created as templates.
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> desk_templates;
  // Saved desks created for save & recall.
  std::vector<raw_ptr<const DeskTemplate, VectorExperimental>> save_and_recall;
};

SavedDesks Group(
    const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>&
        saved_desks) {
  SavedDesks grouped;

  for (const ash::DeskTemplate* saved_desk : saved_desks) {
    switch (saved_desk->type()) {
      case DeskTemplateType::kTemplate:
        grouped.desk_templates.push_back(saved_desk);
        break;
      case DeskTemplateType::kSaveAndRecall:
        grouped.save_and_recall.push_back(saved_desk);
        break;
      // Do nothing in the case of a floating workspace type or an unknown type.
      case DeskTemplateType::kFloatingWorkspace:
      case DeskTemplateType::kUnknown:
        break;
    }
  }

  return grouped;
}

std::unique_ptr<views::View> GetLabelAndGridGroupContents() {
  auto group_contents = std::make_unique<views::View>();
  auto* group_layout =
      group_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(),
          kGroupContentsBetweenChildSpacingDp));
  group_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);
  group_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  return group_contents;
}

}  // namespace

// -----------------------------------------------------------------------------
// SavedDeskLibraryWindowTargeter:

// A custom targeter that only allows us to handle events which are located on
// the children of the library. The library takes up all the available space in
// overview, but we still want some events to fall through to the
// `OverviewGridEventHandler`, if they are not handled by the library's
// children.
class SavedDeskLibraryWindowTargeter : public aura::WindowTargeter {
 public:
  explicit SavedDeskLibraryWindowTargeter(SavedDeskLibraryView* owner)
      : owner_(owner) {}
  SavedDeskLibraryWindowTargeter(const SavedDeskLibraryWindowTargeter&) =
      delete;
  SavedDeskLibraryWindowTargeter& operator=(
      const SavedDeskLibraryWindowTargeter&) = delete;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    // Convert to screen coordinate.
    gfx::Point screen_location;
    gfx::Rect screen_bounds = owner_->GetBoundsInScreen();
    if (event.target()) {
      screen_location = event.target()->GetScreenLocation(event);
    } else {
      screen_location = event.root_location();
      wm::ConvertPointToScreen(window->GetRootWindow(), &screen_location);
    }

    // Do not process if it's not on the library view.
    if (!screen_bounds.Contains(screen_location))
      return false;

    // Process the event if it is for scrolling.
    if (event.IsMouseWheelEvent() || event.IsScrollEvent() ||
        event.IsScrollGestureEvent()) {
      return true;
    }

    // Process the event if it is touch.
    if (event.IsTouchEvent())
      return true;

    // Process the event if it intersects with grid items.
    if (owner_->IntersectsWithUi(screen_location))
      return true;

    // None of the library's children will handle the event, so `window` won't
    // handle the event and it will fall through to the wallpaper.
    return false;
  }

 private:
  const raw_ptr<SavedDeskLibraryView> owner_;
};

// -----------------------------------------------------------------------------
// SavedDeskLibraryEventHandler:

// This class is owned by SavedDeskLibraryView for the purpose of handling mouse
// and gesture events.
class SavedDeskLibraryEventHandler : public ui::EventHandler {
 public:
  explicit SavedDeskLibraryEventHandler(SavedDeskLibraryView* owner)
      : owner_(owner) {}
  SavedDeskLibraryEventHandler(const SavedDeskLibraryEventHandler&) = delete;
  SavedDeskLibraryEventHandler& operator=(const SavedDeskLibraryEventHandler&) =
      delete;
  ~SavedDeskLibraryEventHandler() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/false);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/true);
  }

  void OnKeyEvent(ui::KeyEvent* event) override { owner_->OnKeyEvent(event); }

 private:
  const raw_ptr<SavedDeskLibraryView> owner_;
};

// -----------------------------------------------------------------------------
// SavedDeskLibraryView:

// static
std::unique_ptr<views::Widget>
SavedDeskLibraryView::CreateSavedDeskLibraryWidget(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = true;
  // The parent should be a container that covers all the windows but is below
  // some other system UI features such as system tray and capture mode and also
  // below the system modal dialogs.
  DesksController* desks_controller = DesksController::Get();
  params.parent = desks_controller->GetDeskContainer(
      root, desks_controller->GetDeskIndex(desks_controller->active_desk()));
  params.name = "SavedDeskLibraryWidget";
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  params.init_properties_container.SetProperty(kOverviewUiKey, true);

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SavedDeskLibraryView>());

  // Not opaque since we want to view the contents of the layer behind.
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeWindow(),
                                             wm::ANIMATE_NONE);
  return widget;
}

SavedDeskLibraryView::SavedDeskLibraryView() {
  // The entire page scrolls.
  scroll_view_ = AddChildView(std::make_unique<views::ScrollView>(
      views::ScrollView::ScrollWithLayers::kEnabled));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The overview grid already has one.
  scroll_view_->SetBackgroundColor(std::nullopt);
  scroll_view_->SetAllowKeyboardScrolling(true);

  // Scroll view will have a gradient mask layer.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  // Use ash style rounded scroll bar just like `AppListBubbleAppsPage`.
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kLibraryPageVerticalScrollInsets);
  vertical_scroll->SetSnapBackOnDragOutside(false);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  scroll_view_gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(
      scroll_view_, kScrollViewGradientSize);

  // Set up scroll contents.
  auto scroll_contents = std::make_unique<views::View>();
  auto* layout =
      scroll_contents->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical,
          kLibraryPageScrollContentsInsets,
          kLibraryPageScrollContentsBetweenChildSpacingDp));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Create grids depending on which features are enabled.
  if (saved_desk_util::AreDesksTemplatesEnabled()) {
    auto group_contents = GetLabelAndGridGroupContents();
    desk_template_grid_view_ =
        group_contents->AddChildView(std::make_unique<SavedDeskGridView>());
    grid_views_.push_back(desk_template_grid_view_.get());

    scroll_contents->AddChildView(std::move(group_contents));
  }
  if (saved_desk_util::ShouldShowSavedDesksOptions()) {
    auto group_contents = GetLabelAndGridGroupContents();
    save_and_recall_grid_view_ =
        group_contents->AddChildView(std::make_unique<SavedDeskGridView>());
    grid_views_.push_back(save_and_recall_grid_view_.get());

    scroll_contents->AddChildView(std::move(group_contents));
  }

  no_items_label_ =
      scroll_contents->AddChildView(std::make_unique<RoundedLabel>(
          kNoItemsLabelPadding.width(), kNoItemsLabelPadding.height(),
          kSaveDeskCornerRadius, kNoItemsLabelHeight,
          l10n_util::GetStringUTF16(
              saved_desk_util::AreDesksTemplatesEnabled()
                  ? IDS_ASH_DESKS_TEMPLATES_LIBRARY_NO_TEMPLATES_OR_DESKS_LABEL
                  : IDS_ASH_DESKS_TEMPLATES_LIBRARY_NO_DESKS_LABEL)));
  no_items_label_->SetVisible(false);

  scroll_view_->SetContents(std::move(scroll_contents));
}

SavedDeskLibraryView::~SavedDeskLibraryView() {
  if (auto* widget_window = GetWidgetWindow()) {
    widget_window->RemovePreTargetHandler(event_handler_.get());
    widget_window->RemoveObserver(this);
  }
}

SavedDeskItemView* SavedDeskLibraryView::GetItemForUUID(
    const base::Uuid& uuid) {
  for (ash::SavedDeskGridView* grid_view : grid_views()) {
    if (auto* item = grid_view->GetItemForUUID(uuid))
      return item;
  }
  return nullptr;
}

void SavedDeskLibraryView::AddOrUpdateEntries(
    const std::vector<raw_ptr<const DeskTemplate, VectorExperimental>>& entries,
    const base::Uuid& order_first_uuid,
    bool animate) {
  SavedDesks grouped = Group(entries);
  if (desk_template_grid_view_ && !grouped.desk_templates.empty()) {
    desk_template_grid_view_->AddOrUpdateEntries(grouped.desk_templates,
                                                 order_first_uuid, animate);
  }
  if (save_and_recall_grid_view_ && !grouped.save_and_recall.empty()) {
    save_and_recall_grid_view_->AddOrUpdateEntries(grouped.save_and_recall,
                                                   order_first_uuid, animate);
  }

  DeprecatedLayoutImmediately();
}

void SavedDeskLibraryView::DeleteEntries(const std::vector<base::Uuid>& uuids,
                                         bool delete_animation) {
  if (desk_template_grid_view_)
    desk_template_grid_view_->DeleteEntries(uuids, delete_animation);
  if (save_and_recall_grid_view_)
    save_and_recall_grid_view_->DeleteEntries(uuids, delete_animation);

  DeprecatedLayoutImmediately();
}

void SavedDeskLibraryView::AnimateDeskLaunch(const base::Uuid& uuid,
                                             DeskMiniView* mini_view) {
  SavedDeskItemView* grid_item = GetItemForUUID(uuid);
  DCHECK(grid_item);

  ui::Layer* mini_view_layer = mini_view->layer();
  // Stop any ongoing animations for the mini view before we try to get the
  // target screen bounds of it and start the new animation for it below.
  mini_view_layer->CompleteAllAnimations();

  // If we can't the get bounds, then we just bail. The item will be deleted
  // automatically later through desk model observation.
  std::optional<gfx::Rect> target_screen_bounds =
      GetDeskPreviewBoundsForLaunch(mini_view);
  if (!target_screen_bounds)
    return;

  // Immediately hide the desk mini view. It will later be revealed by the
  // animation below.
  mini_view_layer->SetOpacity(0.0);

  std::unique_ptr<ui::LayerTreeOwner> item_layer_tree =
      wm::RecreateLayers(grid_item);
  ui::Layer* item_layer = item_layer_tree->root();
  GetWidget()->GetLayer()->Add(item_layer);

  const gfx::Rect source_screen_bounds = grid_item->GetBoundsInScreen();

  // Create a transform from `source_screen_bounds` to `target_screen_bounds`.
  gfx::Transform transform;
  transform.Translate(target_screen_bounds->origin() -
                      source_screen_bounds.origin());
  transform.Scale(static_cast<float>(target_screen_bounds->width()) /
                      source_screen_bounds.width(),
                  static_cast<float>(target_screen_bounds->height()) /
                      source_screen_bounds.height());

  views::AnimationBuilder()
      .OnEnded(base::DoNothingWithBoundArgs(std::move(item_layer_tree)))
      .Once()
      // Animating the desk item up to the desk bar.
      .SetDuration(kSaveAndRecallLaunchMoveDuration)
      .SetTransform(item_layer, transform, gfx::Tween::ACCEL_20_DECEL_100)
      // Crossfading the desk item to the desk mini view.
      .Offset(kSaveAndRecallLaunchFadeDelay)
      .SetDuration(kSaveAndRecallLaunchFadeDuration)
      .SetOpacity(mini_view_layer, 1.0f)
      .SetOpacity(item_layer, 0.0f);

  // Delete the existing saved desk item without animation.
  DeleteEntries({uuid}, /*delete_animation=*/false);
}

bool SavedDeskLibraryView::IsAnimating() const {
  for (ash::SavedDeskGridView* grid_view : grid_views()) {
    if (grid_view->IsAnimating())
      return true;
  }

  return false;
}

bool SavedDeskLibraryView::IntersectsWithUi(
    const gfx::Point& screen_location) const {
  // Check saved desk items.
  for (ash::SavedDeskGridView* grid : grid_views()) {
    for (ash::SavedDeskItemView* item : grid->grid_items()) {
      if (item->GetBoundsInScreen().Contains(screen_location))
        return true;
    }
  }
  return false;
}

aura::Window* SavedDeskLibraryView::GetWidgetWindow() {
  auto* widget = GetWidget();
  return widget ? widget->GetNativeWindow() : nullptr;
}

void SavedDeskLibraryView::OnLocatedEvent(ui::LocatedEvent* event,
                                          bool is_touch) {
  auto* widget_window = GetWidgetWindow();
  if (widget_window && widget_window->event_targeting_policy() ==
                           aura::EventTargetingPolicy::kNone) {
    // If this is true, then we're in the process of fading out `this` and don't
    // want to handle any events anymore so do nothing.
    return;
  }

  // We also don't want to handle any events while we are animating.
  if (IsAnimating()) {
    event->StopPropagation();
    event->SetHandled();
    return;
  }

  const gfx::Point screen_location =
      event->target() ? event->target()->GetScreenLocation(*event)
                      : event->root_location();

  switch (event->type()) {
    case ui::EventType::kMouseMoved:
    case ui::EventType::kMouseEntered:
    case ui::EventType::kMouseReleased:
    case ui::EventType::kMouseExited:
    case ui::EventType::kGestureScrollBegin:
    case ui::EventType::kGestureLongPress:
    case ui::EventType::kGestureLongTap: {
      if (event->IsGestureEvent())
        SavedDeskNameView::CommitChanges(GetWidget());

      // For gesture scroll, we don't update hover button visibility but commit
      // name changes for grid items.
      if (event->type() == ui::EventType::kGestureScrollBegin) {
        break;
      }

      for (ash::SavedDeskGridView* grid_view : grid_views()) {
        for (SavedDeskItemView* grid_item : grid_view->grid_items())
          grid_item->UpdateHoverButtonsVisibility(screen_location, is_touch);
      }
      break;
    }
    case ui::EventType::kGestureTap:
      // When it's a tap outside grid items, it should either commit the name
      // change or exit the overview mode. Currently those are handled in
      // `OverviewGrid` for both saved desk library view and desk bar
      // view. `OverviewGridEventHandler::HandleClickOrTap()` is explicitly
      // invoked here because `ScrollBar::OnGestureEvent()` would eat tap
      // event. With this, it would lose the gesture-triggered context menu in
      // saved desk library view. Please see crbug.com/1339100.
      // TODO(crbug.com/1341128): Investigate if we can enable the context menu
      // via long-press in library page.
      if (!IntersectsWithUi(screen_location)) {
        Shell::Get()
            ->overview_controller()
            ->overview_session()
            ->GetGridWithRootWindow(widget_window->GetRootWindow())
            ->grid_event_handler()
            ->HandleClickOrTap(event);
        event->StopPropagation();
        event->SetHandled();
      }
      break;
    default:
      break;
  }
}

std::optional<gfx::Rect> SavedDeskLibraryView::GetDeskPreviewBoundsForLaunch(
    const DeskMiniView* mini_view) {
  gfx::Rect desk_preview_bounds =
      mini_view->desk_preview()->GetBoundsInScreen();
  if (std::optional<gfx::Point> desk_preview_origin =
          mini_view->layer()->transform().InverseMapPoint(
              desk_preview_bounds.origin())) {
    return gfx::Rect(*desk_preview_origin, desk_preview_bounds.size());
  }
  return std::nullopt;
}

void SavedDeskLibraryView::AddedToWidget() {
  event_handler_ = std::make_unique<SavedDeskLibraryEventHandler>(this);

  auto* widget_window = GetWidgetWindow();
  DCHECK(widget_window);
  widget_window->AddObserver(this);
  widget_window->AddPreTargetHandler(event_handler_.get());
  widget_window->SetEventTargeter(
      std::make_unique<SavedDeskLibraryWindowTargeter>(this));
}

void SavedDeskLibraryView::Layout(PassKey) {
  if (bounds().IsEmpty())
    return;

  const bool landscape = width() >= kLandscapeMinWidth;
  size_t total_saved_desks = 0;
  for (ash::SavedDeskGridView* grid_view : grid_views()) {
    grid_view->set_layout_mode(landscape
                                   ? SavedDeskGridView::LayoutMode::LANDSCAPE
                                   : SavedDeskGridView::LayoutMode::PORTRAIT);
    total_saved_desks += grid_view->grid_items().size();
  }

  no_items_label_->SetVisible(total_saved_desks == 0);

  scroll_view_->SetBoundsRect({0, 0, width(), height()});
  scroll_view_gradient_helper_->UpdateGradientMask();
}

void SavedDeskLibraryView::OnKeyEvent(ui::KeyEvent* event) {
  bool is_scrolling_event;
  switch (event->key_code()) {
    case ui::VKEY_HOME:
    case ui::VKEY_END:
      is_scrolling_event = true;
      // Do not process if home/end key are for text editing.
      for (SavedDeskGridView* grid_view : grid_views_) {
        if (grid_view->IsSavedDeskNameBeingModified()) {
          is_scrolling_event = false;
          break;
        }
      }
      break;
    case ui::VKEY_PRIOR:
    case ui::VKEY_NEXT:
      is_scrolling_event = true;
      break;
    default:
      // Ignore all other key events as arrow keys are used for moving focus.
      is_scrolling_event = false;
      break;
  }
  if (is_scrolling_event)
    scroll_view_->vertical_scroll_bar()->OnKeyEvent(event);
}

void SavedDeskLibraryView::OnWindowDestroying(aura::Window* window) {
  auto* widget_window = GetWidgetWindow();
  DCHECK_EQ(window, widget_window);
  DCHECK(event_handler_);
  widget_window->RemovePreTargetHandler(event_handler_.get());
  widget_window->RemoveObserver(this);
  event_handler_ = nullptr;
}

BEGIN_METADATA(SavedDeskLibraryView)
END_METADATA

}  // namespace ash
