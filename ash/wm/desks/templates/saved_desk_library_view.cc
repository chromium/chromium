// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/saved_desk_library_view.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"

namespace ash {
namespace {

// Vertical spacing between the last grid item and the feedback button.
constexpr int kFeedbackButtonSpacingDp = 40;

struct SavedDesks {
  // Saved desks created as templates.
  std::vector<const DeskTemplate*> desk_templates;
  // Saved desks created for save & recall.
  std::vector<const DeskTemplate*> save_and_recall;
};

SavedDesks Group(const std::vector<const DeskTemplate*>& saved_desks) {
  SavedDesks grouped;

  for (auto* saved_desk : saved_desks) {
    switch (saved_desk->type()) {
      case DeskTemplateType::kTemplate:
        grouped.desk_templates.push_back(saved_desk);
        break;
      case DeskTemplateType::kSaveAndRecall:
        grouped.save_and_recall.push_back(saved_desk);
        break;
    }
  }

  return grouped;
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
  SavedDeskLibraryWindowTargeter(SavedDeskLibraryView* owner) : owner_(owner) {}
  SavedDeskLibraryWindowTargeter(const SavedDeskLibraryWindowTargeter&) =
      delete;
  SavedDeskLibraryWindowTargeter& operator=(
      const SavedDeskLibraryWindowTargeter&) = delete;

  // aura::WindowTargeter:
  bool SubtreeShouldBeExploredForEvent(aura::Window* window,
                                       const ui::LocatedEvent& event) override {
    if (!owner_->IntersectsWithUi(event.location()))
      return false;

    // None of the libary's children will handle the event, so `window` won't
    // handle the event and it will fall through to the wallpaper.
    return aura::WindowTargeter::SubtreeShouldBeExploredForEvent(window, event);
  }

 private:
  SavedDeskLibraryView* const owner_;
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

 private:
  SavedDeskLibraryView* const owner_;
};

// -----------------------------------------------------------------------------
// SavedDeskLibraryView:

// static
std::unique_ptr<views::Widget>
SavedDeskLibraryView::CreateSavedDeskLibraryWidget(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  // The parent should be a container that covers all the windows but is below
  // some other system UI features such as system tray and capture mode and also
  // below the system modal dialogs.
  // TODO(sammiequon): Investigate if there is a more suitable container for
  // this widget.
  params.parent = root->GetChildById(kShellWindowId_ShelfBubbleContainer);
  params.name = "SavedDeskLibraryWidget";

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<SavedDeskLibraryView>());

  // Not opaque since we want to view the contents of the layer behind.
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  widget->GetNativeWindow()->SetId(kShellWindowId_SavedDeskLibraryWindow);

  return widget;
}

SavedDeskLibraryView::SavedDeskLibraryView()
    : bounds_animator_(this, /*use_transforms=*/true) {
  // Create grids depending on which features are enabled.
  if (features::AreDesksTemplatesEnabled()) {
    desk_template_grid_view_ =
        AddChildView(std::make_unique<SavedDeskGridView>());
    grid_views_.push_back(desk_template_grid_view_);
  }
  if (features::IsSavedDesksEnabled()) {
    save_and_recall_grid_view_ =
        AddChildView(std::make_unique<SavedDeskGridView>());
    grid_views_.push_back(save_and_recall_grid_view_);
  }

  feedback_button_ = AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&SavedDeskLibraryView::OnFeedbackButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_FEEDBACK),
      PillButton::Type::kIcon, &kPersistentDesksBarFeedbackIcon));
}

SavedDeskLibraryView::~SavedDeskLibraryView() {
  if (auto* widget_window = GetWidgetWindow()) {
    widget_window->RemovePreTargetHandler(event_handler_.get());
    widget_window->RemoveObserver(this);
  }
}

SavedDeskItemView* SavedDeskLibraryView::GetItemForUUID(
    const base::GUID& uuid) {
  for (auto* grid_view : grid_views()) {
    if (auto* item = grid_view->GetItemForUUID(uuid))
      return item;
  }
  return nullptr;
}

void SavedDeskLibraryView::PopulateGridUI(
    const std::vector<const DeskTemplate*>& entries,
    const gfx::Rect& grid_bounds,
    const base::GUID& last_saved_desk_uuid) {
  GetWidget()->SetBounds(grid_bounds);

  SavedDesks grouped_entries = Group(entries);
  if (desk_template_grid_view_) {
    desk_template_grid_view_->PopulateGridUI(grouped_entries.desk_templates,
                                             last_saved_desk_uuid);
  }
  if (save_and_recall_grid_view_) {
    save_and_recall_grid_view_->PopulateGridUI(grouped_entries.save_and_recall,
                                               last_saved_desk_uuid);
  }

  AnimateItems();
}

void SavedDeskLibraryView::AddOrUpdateTemplates(
    const std::vector<const DeskTemplate*>& entries,
    bool initializing_grid_view,
    const base::GUID& last_saved_desk_uuid) {
  SavedDesks grouped_entries = Group(entries);
  if (desk_template_grid_view_) {
    desk_template_grid_view_->AddOrUpdateTemplates(
        grouped_entries.desk_templates, initializing_grid_view,
        last_saved_desk_uuid);
  }
  if (save_and_recall_grid_view_) {
    save_and_recall_grid_view_->AddOrUpdateTemplates(
        grouped_entries.save_and_recall, initializing_grid_view,
        last_saved_desk_uuid);
  }

  AnimateItems();
}

void SavedDeskLibraryView::DeleteTemplates(
    const std::vector<std::string>& uuids) {
  if (desk_template_grid_view_)
    desk_template_grid_view_->DeleteTemplates(uuids);
  if (save_and_recall_grid_view_)
    save_and_recall_grid_view_->DeleteTemplates(uuids);

  AnimateItems();
}

std::vector<std::pair<views::View*, gfx::Rect>>
SavedDeskLibraryView::CalculatePositions() const {
  // TODO(dandersson): The following code is entirely temporary. It's made so
  // that the positioning of desk templates doesn't regress. The positioning of
  // the save & recall grid (if present), is not final.
  std::vector<std::pair<views::View*, gfx::Rect>> positions;
  if (bounds().IsEmpty())
    return positions;

  constexpr int kGridPadding = 50;  // Padding between grids.

  // Total height of all grids, including padding.
  int total_height = 0;

  std::vector<int> grid_heights(grid_views_.size());
  for (size_t i = 0; i != grid_views_.size(); ++i) {
    const int grid_height = grid_views_[i]->GetSizeForWidth(width()).height();
    grid_heights[i] = grid_height;
    total_height += grid_height;
  }

  const size_t non_empty_grids =
      std::count_if(grid_heights.begin(), grid_heights.end(),
                    [](int height) { return height > 0; });

  // Add space for padding.
  total_height +=
      non_empty_grids > 1 ? (non_empty_grids - 1) * kGridPadding : 0;

  int y_pos = bounds().height() / 2 - total_height / 2;
  for (size_t i = 0; i != grid_views_.size(); ++i) {
    positions.emplace_back(
        grid_views_[i], gfx::Rect(0, y_pos, bounds().width(), grid_heights[i]));

    y_pos += grid_heights[i];
    if (grid_heights[i])
      y_pos += kGridPadding;
  }

  // Position of feedback button.
  if (total_height == 0) {
    positions.emplace_back(feedback_button_, feedback_button_->bounds());
  } else {
    const gfx::Size feedback_size = feedback_button_->GetPreferredSize();
    positions.emplace_back(
        feedback_button_,
        gfx::Rect(gfx::Point(width() / 2 - feedback_size.width() / 2,
                             y_pos - kGridPadding + kFeedbackButtonSpacingDp),
                  feedback_size));
  }

  return positions;
}

void SavedDeskLibraryView::OnFeedbackButtonPressed() {
  std::string extra_diagnostics;
  for (auto* grid : grid_views()) {
    for (auto* item : grid->grid_items())
      extra_diagnostics += (item->desk_template()->ToString() + "\n");
  }

  // Note that this will activate the dialog which will exit overview and delete
  // `this`.
  Shell::Get()->desks_templates_delegate()->OpenFeedbackDialog(
      extra_diagnostics);
}

void SavedDeskLibraryView::AnimateItems() {
  for (const auto& [view, bounds] : CalculatePositions()) {
    const gfx::Rect target_bounds = bounds_animator_.GetTargetBounds(view);
    if (target_bounds.IsEmpty() || target_bounds == bounds)
      view->SetBoundsRect(bounds);
    else
      bounds_animator_.AnimateViewTo(view, bounds);
  }
}

bool SavedDeskLibraryView::IsAnimating() {
  if (bounds_animator_.IsAnimating())
    return true;

  for (auto* grid_view : grid_views()) {
    if (grid_view->IsAnimating())
      return true;
  }

  return false;
}

bool SavedDeskLibraryView::IntersectsWithUi(const gfx::Point& location) {
  // Check saved desk items.
  for (auto* grid : grid_views()) {
    for (auto* item : grid->grid_items()) {
      if (item->GetBoundsInScreen().Contains(location))
        return true;
    }
  }
  // Check feedback button.
  return feedback_button_->GetBoundsInScreen().Contains(location);
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

  switch (event->type()) {
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_ENTERED:
    case ui::ET_MOUSE_RELEASED:
    case ui::ET_MOUSE_EXITED:
    case ui::ET_GESTURE_LONG_PRESS:
    case ui::ET_GESTURE_LONG_TAP: {
      const gfx::Point screen_location =
          event->target() ? event->target()->GetScreenLocation(*event)
                          : event->root_location();
      for (auto* grid_view : grid_views()) {
        for (SavedDeskItemView* grid_item : grid_view->grid_items())
          grid_item->UpdateHoverButtonsVisibility(screen_location, is_touch);
      }
      break;
    }
    default:
      break;
  }
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

void SavedDeskLibraryView::Layout() {
  AnimateItems();
}

void SavedDeskLibraryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  feedback_button_->SetBackgroundColor(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80));
}

void SavedDeskLibraryView::OnWindowDestroying(aura::Window* window) {
  auto* widget_window = GetWidgetWindow();
  DCHECK_EQ(window, widget_window);
  DCHECK(event_handler_);
  widget_window->RemovePreTargetHandler(event_handler_.get());
  widget_window->RemoveObserver(this);
  event_handler_ = nullptr;
}

BEGIN_METADATA(SavedDeskLibraryView, views::View)
END_METADATA

}  // namespace ash
