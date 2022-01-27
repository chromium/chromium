// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_grid_view.h"

#include <algorithm>
#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/desks_templates_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/templates/desks_templates_animations.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Maximum number of columns for items when width >= height.
constexpr int kLandscapeMaxColumns = 3;
// Ditto for when width < height.
constexpr int kPortraitMaxColumns = 2;

// TODO(richui): Replace these temporary values once specs come out.
constexpr int kGridPaddingDp = 25;

constexpr int kFeedbackButtonSpacingDp = 40;

}  // namespace

// -----------------------------------------------------------------------------
// DesksTemplatesEventHandler:

// This class is owned by DesksTemplatesGridView for the purpose of handling
// mouse and gesture events.
class DesksTemplatesEventHandler : public ui::EventHandler {
 public:
  explicit DesksTemplatesEventHandler(DesksTemplatesGridView* owner)
      : owner_(owner) {}
  DesksTemplatesEventHandler(const DesksTemplatesEventHandler&) = delete;
  DesksTemplatesEventHandler& operator=(const DesksTemplatesEventHandler&) =
      delete;
  ~DesksTemplatesEventHandler() override = default;

  void OnMouseEvent(ui::MouseEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/false);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    owner_->OnLocatedEvent(event, /*is_touch=*/true);
  }

 private:
  DesksTemplatesGridView* const owner_;
};

// -----------------------------------------------------------------------------
// DesksTemplatesGridView:

DesksTemplatesGridView::DesksTemplatesGridView() = default;

DesksTemplatesGridView::~DesksTemplatesGridView() {
  if (widget_window_) {
    widget_window_->RemovePreTargetHandler(event_handler_.get());
    widget_window_->RemoveObserver(this);
  }
}

// static
std::unique_ptr<views::Widget>
DesksTemplatesGridView::CreateDesksTemplatesGridWidget(aura::Window* root) {
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
  params.name = "DesksTemplatesGridWidget";

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->SetContentsView(std::make_unique<DesksTemplatesGridView>());

  // Not opaque since we want to view the contents of the layer behind.
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  widget->GetNativeWindow()->SetId(kShellWindowId_DesksTemplatesGridWindow);

  return widget;
}

void DesksTemplatesGridView::UpdateGridUI(
    const std::vector<DeskTemplate*>& desk_templates,
    const gfx::Rect& grid_bounds) {
  // Check if any of the template items or their name views have overview focus
  // and notify the highlight controller. This should only be needed when a
  // template item is deleted, but currently we call `UpdateGridUI` every time
  // the model changes.
  // TODO(richui): Remove this when `UpdateGridUI` is not rebuilt every time.
  if (!grid_items_.empty()) {
    auto* highlight_controller = Shell::Get()
                                     ->overview_controller()
                                     ->overview_session()
                                     ->highlight_controller();
    if (highlight_controller->IsFocusHighlightVisible()) {
      // Notify the highlight controller if any of the about to be destroyed
      // views have overview focus to prevent use-after-free.
      for (DesksTemplatesItemView* template_view : grid_items_) {
        if (template_view->IsViewHighlighted()) {
          highlight_controller->OnViewDestroyingOrDisabling(template_view);
          break;
        }

        if (template_view->name_view()->IsViewHighlighted()) {
          highlight_controller->OnViewDestroyingOrDisabling(
              template_view->name_view());
          break;
        }
      }
    }
  }

  RemoveAllChildViews();
  grid_items_.clear();

  if (desk_templates.empty())
    return;

  DCHECK_LE(desk_templates.size(),
            DesksTemplatesPresenter::Get()->GetMaxEntryCount());

  std::vector<std::unique_ptr<DesksTemplatesItemView>> desk_template_views;

  for (DeskTemplate* desk_template : desk_templates) {
    desk_template_views.push_back(
        std::make_unique<DesksTemplatesItemView>(desk_template));
  }

  // Sort the `desk_template_views` into alphabetical order based on template
  // name, note that accessible name == template name.
  std::sort(desk_template_views.begin(), desk_template_views.end(),
            [](const std::unique_ptr<DesksTemplatesItemView>& a,
               const std::unique_ptr<DesksTemplatesItemView>& b) {
              return a->GetAccessibleName() < b->GetAccessibleName();
            });

  // Add each of the templates to the grid.
  for (auto& view : desk_template_views)
    grid_items_.push_back(AddChildView(std::move(view)));

  feedback_button_ = AddChildView(std::make_unique<PillButton>(
      base::BindRepeating(&DesksTemplatesGridView::OnFeedbackButtonPressed,
                          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_FEEDBACK),
      PillButton::Type::kIcon, &kPersistentDesksBarFeedbackIcon));

  const gfx::Size previous_size = size();

  GetWidget()->SetBounds(grid_bounds);

  // The children won't be layoutted if the size remains the same, which may
  // happen when we reshow the widget after it was hidden. Force a layout in
  // this case. If the size changes, the children will be layoutted so we can
  // avoid double work in that case. See https://crbug.com/1275179.
  if (size() == previous_size)
    Layout();
}

bool DesksTemplatesGridView::IsTemplateNameBeingModified() const {
  if (!GetWidget()->IsActive())
    return false;

  for (auto* grid_item : grid_items_) {
    if (grid_item->IsTemplateNameBeingModified())
      return true;
  }
  return false;
}

void DesksTemplatesGridView::Layout() {
  if (grid_items_.empty())
    return;

  const size_t count = grid_items_.size();
  const gfx::Size grid_item_size = grid_items_[0]->GetPreferredSize();
  const size_t max_column_count =
      width() >= height() ? kLandscapeMaxColumns : kPortraitMaxColumns;
  const size_t column_count = std::min(count, max_column_count);
  const size_t row_count =
      (count / max_column_count) + ((count % max_column_count) == 0 ? 0 : 1);
  const int total_width =
      column_count * (grid_item_size.width() + kGridPaddingDp) - kGridPaddingDp;
  const int total_height =
      row_count * (grid_item_size.height() + kGridPaddingDp) - kGridPaddingDp;

  const int initial_x = (width() - total_width) / 2;
  int x = initial_x;
  int y = (height() - total_height) / 2;

  for (size_t i = 0; i < count; i++) {
    if (i != 0 && i % column_count == 0) {
      // Move the position to the start of the next row.
      x = initial_x;
      y += grid_item_size.height() + kGridPaddingDp;
    }

    grid_items_[i]->SetBoundsRect(gfx::Rect(gfx::Point(x, y), grid_item_size));

    x += grid_item_size.width() + kGridPaddingDp;
  }

  if (feedback_button_) {
    // The feedback button is centered and `kFeedbackButtonSpacingDp` from the
    // bottom most grid item.
    const gfx::Size feedback_size = feedback_button_->GetPreferredSize();
    feedback_button_->SetBoundsRect(gfx::Rect(
        gfx::Point(
            width() / 2 - feedback_size.width() / 2,
            grid_items_.back()->bounds().bottom() + kFeedbackButtonSpacingDp),
        feedback_size));
  }
}

void DesksTemplatesGridView::AddedToWidget() {
  // Adding a pre-target handler to ensure that events are not accidentally
  // captured by the child views. Also, an `EventHandler`
  // (DesksTemplatesEventHandler) is added as the pre-target handler to the
  // window as opposed to `Env` to ensure that we only get events that are on
  // this window.
  event_handler_ = std::make_unique<DesksTemplatesEventHandler>(this);
  widget_window_ = GetWidget()->GetNativeWindow();
  widget_window_->AddObserver(this);
  widget_window_->AddPreTargetHandler(event_handler_.get());
}

void DesksTemplatesGridView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, widget_window_);
  DCHECK(event_handler_);
  widget_window_->RemovePreTargetHandler(event_handler_.get());
  widget_window_->RemoveObserver(this);
  event_handler_.reset();
  widget_window_ = nullptr;
}

void DesksTemplatesGridView::OnLocatedEvent(ui::LocatedEvent* event,
                                            bool is_touch) {
  if (widget_window_ && widget_window_->event_targeting_policy() ==
                            aura::EventTargetingPolicy::kNone) {
    // If this is true, then we're in the process of fading out `this` and don't
    // want to handle any events anymore so do nothing.
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
      for (DesksTemplatesItemView* grid_item : grid_items_)
        grid_item->UpdateHoverButtonsVisibility(screen_location, is_touch);
      return;
    }
    default:
      return;
  }
}

void DesksTemplatesGridView::OnFeedbackButtonPressed() {
  std::string extra_diagnostics;
  for (DesksTemplatesItemView* grid_item : grid_items_)
    extra_diagnostics += (grid_item->desk_template()->ToString() + "\n");

  // Note that this will activate the dialog which will exit overview and delete
  // `this`.
  Shell::Get()->desks_templates_delegate()->OpenFeedbackDialog(
      extra_diagnostics);
}

BEGIN_METADATA(DesksTemplatesGridView, views::View)
END_METADATA

}  // namespace ash
