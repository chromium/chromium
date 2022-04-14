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
#include "ash/style/ash_color_provider.h"
#include "ash/style/pill_button.h"
#include "ash/wm/desks/templates/desks_templates_animations.h"
#include "ash/wm/desks/templates/desks_templates_item_view.h"
#include "ash/wm/desks/templates/desks_templates_name_view.h"
#include "ash/wm/desks/templates/desks_templates_presenter.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/i18n/string_compare.h"
#include "third_party/icu/source/common/unicode/uloc.h"
#include "third_party/icu/source/i18n/unicode/coll.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Items are laid out in landscape mode when the aspect ratio of the view is
// above this number.
constexpr float kAspectRatioLimit = 1.38f;
constexpr int kLandscapeMaxColumns = 3;
constexpr int kPortraitMaxColumns = 2;

constexpr int kGridPaddingDp = 25;

constexpr int kFeedbackButtonSpacingDp = 40;

// This is the maximum number of templates we will show in the grid. This
// constant is used instead of the Desk model `GetMaxEntryCount()` because that
// takes into consideration the number of `policy_entries_`, which can cause it
// to exceed 6 items.
// Note: Because we are only showing a maximum number of templates, there are
// cases that not all existing templates will be displayed, such as when a user
// has more than the maximum count. Since we also don't update the grid whenever
// there is a change, deleting a template may result in existing templates not
// being shown as well, if the user originally exceeded the max template count
// when the grid was first shown.
constexpr std::size_t kMaxTemplateCount = 6u;

constexpr gfx::Transform kEndTransform;

// Scale for adding/deleting grid items.
constexpr float kAddOrDeleteItemScale = 0.75f;

constexpr base::TimeDelta kBoundsChangeAnimationDuration =
    base::Milliseconds(300);

constexpr base::TimeDelta kTemplateViewsScaleAndFadeDuration =
    base::Milliseconds(50);

// Gets the scale transform for `view`. It returns a transform with a scale of
// `kAddOrDeleteItemScale`. The pivot of the scale animation will be the center
// point of the view.
gfx::Transform GetScaleTransformForView(views::View* view) {
  gfx::Transform scale_transform;
  scale_transform.Scale(kAddOrDeleteItemScale, kAddOrDeleteItemScale);
  return gfx::TransformAboutPivot(view->GetLocalBounds().CenterPoint(),
                                  scale_transform);
}

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

DesksTemplatesGridView::DesksTemplatesGridView()
    : bounds_animator_(this, /*use_transforms=*/true) {
  // Bounds animator is unaffected by debug tools such as "--ui-slow-animations"
  // flag, so manually multiply the duration here.
  bounds_animator_.SetAnimationDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() *
      kBoundsChangeAnimationDuration);
  bounds_animator_.set_tween_type(gfx::Tween::LINEAR);
}

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

void DesksTemplatesGridView::PopulateGridUI(
    const std::vector<DeskTemplate*>& desk_templates,
    const gfx::Rect& grid_bounds,
    const base::GUID& last_saved_template_uuid) {
  DCHECK(grid_items_.empty());

  // TODO(richui|sammiequon): See if this can be removed as this function should
  // only be called once per overview session.
  if (desk_templates.empty()) {
    RemoveAllChildViews();
    grid_items_.clear();
    return;
  }

  AddOrUpdateTemplates(std::vector<const DeskTemplate*>(desk_templates.begin(),
                                                        desk_templates.end()),
                       /*initializing_grid_view=*/true,
                       last_saved_template_uuid);

  if (!feedback_button_) {
    feedback_button_ = AddChildView(std::make_unique<PillButton>(
        base::BindRepeating(&DesksTemplatesGridView::OnFeedbackButtonPressed,
                            base::Unretained(this)),
        l10n_util::GetStringUTF16(
            IDS_ASH_PERSISTENT_DESKS_BAR_CONTEXT_MENU_FEEDBACK),
        PillButton::Type::kIcon, &kPersistentDesksBarFeedbackIcon));
  }

  GetWidget()->SetBounds(grid_bounds);
}

void DesksTemplatesGridView::SortTemplateGridItems(
    const base::GUID& last_saved_template_uuid) {
  // Sort the `grid_items_` into alphabetical order based on template name.
  // Note that this doesn't update the order of the child views, but just sorts
  // the vector. `Layout` is responsible for placing the views in the correct
  // locations in the grid.
  UErrorCode error_code = U_ZERO_ERROR;
  std::unique_ptr<icu::Collator> collator(
      icu::Collator::createInstance(error_code));  // Use current ICU locale.
  DCHECK(U_SUCCESS(error_code));
  // If there is a newly saved template, move that template to the front of the
  // grid, and sort the rest of the templates after it.
  std::sort(
      grid_items_.begin(), grid_items_.end(),
      [&collator, last_saved_template_uuid](const DesksTemplatesItemView* a,
                                            const DesksTemplatesItemView* b) {
        if (last_saved_template_uuid.is_valid() &&
            a->uuid() == last_saved_template_uuid) {
          return true;
        }
        if (last_saved_template_uuid.is_valid() &&
            b->uuid() == last_saved_template_uuid) {
          return false;
        }
        return base::i18n::CompareString16WithCollator(
                   *collator, a->name_view()->GetAccessibleName(),
                   b->name_view()->GetAccessibleName()) < 0;
      });

  // A11y traverses views based on the order of the children, so we need to
  // manually reorder the child views to match the order that they are
  // displayed, which is the alphabetically sorted `grid_items_` order. If
  // there was a newly saved template, the first template in the grid will
  // be the new template, while the rest will be sorted alphabetically.
  for (size_t i = 0; i < grid_items_.size(); i++)
    ReorderChildView(grid_items_[i], i);

  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();
  Layout();
}

void DesksTemplatesGridView::AddOrUpdateTemplates(
    const std::vector<const DeskTemplate*>& entries,
    bool initializing_grid_view,
    const base::GUID& last_saved_template_uuid) {
  std::vector<DesksTemplatesItemView*> new_grid_items;

  for (const DeskTemplate* entry : entries) {
    auto iter = std::find_if(grid_items_.begin(), grid_items_.end(),
                             [entry](DesksTemplatesItemView* grid_item) {
                               return entry->uuid() == grid_item->uuid();
                             });

    if (iter != grid_items_.end()) {
      (*iter)->UpdateTemplate(*entry);
    } else if (grid_items_.size() < kMaxTemplateCount) {
      DesksTemplatesItemView* grid_item =
          AddChildView(std::make_unique<DesksTemplatesItemView>(entry));
      grid_items_.push_back(grid_item);
      if (!initializing_grid_view)
        new_grid_items.push_back(grid_item);
    }
  }

  // Sort the `grid_items_` into alphabetical order based on template name. If a
  // given uuid is valid, it'll push that template item to the front of the grid
  // and sort the remaining templates after it.
  SortTemplateGridItems(last_saved_template_uuid);

  if (!initializing_grid_view) {
    AnimateGridItems(new_grid_items);
    NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
  }
}

void DesksTemplatesGridView::DeleteTemplates(
    const std::vector<std::string>& uuids) {
  OverviewHighlightController* highlight_controller =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->highlight_controller();
  DCHECK(highlight_controller);

  for (const std::string& uuid : uuids) {
    auto iter =
        std::find_if(grid_items_.begin(), grid_items_.end(),
                     [uuid](DesksTemplatesItemView* grid_item) {
                       return uuid == grid_item->uuid().AsLowercaseString();
                     });

    if (iter == grid_items_.end())
      continue;

    DesksTemplatesItemView* grid_item = *iter;
    highlight_controller->OnViewDestroyingOrDisabling(grid_item);
    highlight_controller->OnViewDestroyingOrDisabling(grid_item->name_view());

    // Performs an animation of changing the deleted grid item opacity
    // from 1 to 0 and scales down to `kAddOrDeleteItemScale`. `old_layer_tree`
    // will be deleted when the animation is complete.
    auto old_grid_item_layer_tree = wm::RecreateLayers(grid_item);
    auto* old_grid_item_layer_tree_root = old_grid_item_layer_tree->root();
    GetWidget()->GetLayer()->Add(old_grid_item_layer_tree_root);

    views::AnimationBuilder()
        .OnEnded(base::BindOnce(
            [](std::unique_ptr<ui::LayerTreeOwner> layer_tree_owner) {},
            std::move(old_grid_item_layer_tree)))
        .Once()
        .SetTransform(old_grid_item_layer_tree_root,
                      GetScaleTransformForView(grid_item))
        .SetOpacity(old_grid_item_layer_tree_root, 0.f)
        .SetDuration(kTemplateViewsScaleAndFadeDuration);

    RemoveChildViewT(grid_item);
    grid_items_.erase(iter);
  }

  AnimateGridItems(/*new_grid_items=*/{});
  NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged, true);
}

DesksTemplatesItemView* DesksTemplatesGridView::GridItemBeingModified() {
  if (!GetWidget()->IsActive())
    return nullptr;

  for (auto* grid_item : grid_items_) {
    if (grid_item->IsTemplateNameBeingModified())
      return grid_item;
  }
  return nullptr;
}

void DesksTemplatesGridView::Layout() {
  if (grid_items_.empty())
    return;

  if (bounds_animator_.IsAnimating())
    return;

  const std::vector<gfx::Rect> positions = CalculateGridItemPositions();
  for (size_t i = 0; i < grid_items_.size(); i++)
    grid_items_[i]->SetBoundsRect(positions[i]);

  if (feedback_button_)
    feedback_button_->SetBoundsRect(CalculateFeedbackButtonPosition());
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

void DesksTemplatesGridView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  // In the event where the bounds change while an animation is in progress
  // (i.e. screen rotation), we need to ensure that we stop the current
  // animation. This is because we block layouts while an animation is in
  // progress.
  if (bounds_animator_.IsAnimating())
    bounds_animator_.Cancel();
}

void DesksTemplatesGridView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (feedback_button_) {
    feedback_button_->SetBackgroundColor(
        AshColorProvider::Get()->GetBaseLayerColor(
            AshColorProvider::BaseLayerType::kTransparent80));
  }
}

void DesksTemplatesGridView::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, widget_window_);
  DCHECK(event_handler_);
  widget_window_->RemovePreTargetHandler(event_handler_.get());
  widget_window_->RemoveObserver(this);
  event_handler_.reset();
  widget_window_ = nullptr;
}

bool DesksTemplatesGridView::IntersectsWithFeedbackButton(
    const gfx::Point& point_in_screen) {
  return feedback_button_ &&
         feedback_button_->bounds().Contains(point_in_screen);
}

bool DesksTemplatesGridView::IntersectsWithGridItem(
    const gfx::Point& point_in_screen) {
  for (DesksTemplatesItemView* grid_item : grid_items_) {
    if (grid_item->bounds().Contains(point_in_screen))
      return true;
  }
  return false;
}

DesksTemplatesItemView* DesksTemplatesGridView::GetItemForUUID(
    const base::GUID& uuid) {
  if (!uuid.is_valid())
    return nullptr;

  auto it = std::find_if(grid_items_.begin(), grid_items_.end(),
                         [&uuid](DesksTemplatesItemView* item_view) {
                           return uuid == item_view->desk_template()->uuid();
                         });
  return it == grid_items_.end() ? nullptr : *it;
}

void DesksTemplatesGridView::OnLocatedEvent(ui::LocatedEvent* event,
                                            bool is_touch) {
  if (widget_window_ && widget_window_->event_targeting_policy() ==
                            aura::EventTargetingPolicy::kNone) {
    // If this is true, then we're in the process of fading out `this` and don't
    // want to handle any events anymore so do nothing.
    return;
  }

  // We also don't want to handle any events while we are animating the template
  // view positions.
  if (bounds_animator_.IsAnimating()) {
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
      for (DesksTemplatesItemView* grid_item : grid_items_)
        grid_item->UpdateHoverButtonsVisibility(screen_location, is_touch);
      break;
    }
    default:
      break;
  }

  // If the event is `ui::ET_MOUSE_RELEASED` or `ui::ET_GESTURE_TAP`, it might
  // be a click/tap outside grid item and feedback button to exit overview or
  // to commit name changes.
  if (event->type() == ui::ET_MOUSE_RELEASED ||
      event->type() == ui::ET_GESTURE_TAP) {
    DesksTemplatesItemView* grid_item_being_modified = GridItemBeingModified();
    if (grid_item_being_modified &&
        !grid_item_being_modified->bounds().Contains(event->location())) {
      // When there is a desk grid template name being modified, and the
      // location is outside of the current grid item, commit the name change.
      DesksTemplatesNameView::CommitChanges(GetWidget());
      event->StopPropagation();
      event->SetHandled();
      return;
    }
    if (!grid_item_being_modified &&
        !IntersectsWithGridItem(event->location()) &&
        !IntersectsWithFeedbackButton(event->location())) {
      // When there is no desk grid template name being modified, and the
      // location does not intersect with any grid item or the feedback button,
      // exit overview.
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kClickingOutsideWindowsInOverview);
      event->StopPropagation();
      event->SetHandled();
      return;
    }
  }
}

std::vector<gfx::Rect> DesksTemplatesGridView::CalculateGridItemPositions()
    const {
  std::vector<gfx::Rect> positions;

  if (grid_items_.empty())
    return positions;

  const size_t count = grid_items_.size();
  const gfx::Size grid_item_size = grid_items_[0]->GetPreferredSize();
  const float aspect_ratio =
      static_cast<float>(width()) / std::max(height(), 1);
  const size_t max_column_count = aspect_ratio > kAspectRatioLimit
                                      ? kLandscapeMaxColumns
                                      : kPortraitMaxColumns;
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

    positions.emplace_back(gfx::Point(x, y), grid_item_size);

    x += grid_item_size.width() + kGridPaddingDp;
  }

  DCHECK_EQ(positions.size(), grid_items_.size());

  return positions;
}

gfx::Rect DesksTemplatesGridView::CalculateFeedbackButtonPosition() const {
  // Use the current bounds if the grid is empty. When this happens, the grid
  // will fade out and the feedback button will not be moved.
  if (grid_items_.empty())
    return feedback_button_->bounds();

  // The feedback button is centered and `kFeedbackButtonSpacingDp` from the
  // bottom most grid item.
  const gfx::Size feedback_size = feedback_button_->GetPreferredSize();
  return gfx::Rect(
      gfx::Point(width() / 2 - feedback_size.width() / 2,
                 bounds_animator_.GetTargetBounds(grid_items_.back()).bottom() +
                     kFeedbackButtonSpacingDp),
      feedback_size);
}

void DesksTemplatesGridView::AnimateGridItems(
    const std::vector<DesksTemplatesItemView*>& new_grid_items) {
  const std::vector<gfx::Rect> positions = CalculateGridItemPositions();
  for (size_t i = 0; i < grid_items_.size(); i++) {
    DesksTemplatesItemView* grid_item = grid_items_[i];
    const gfx::Rect target_bounds = positions[i];
    if (bounds_animator_.GetTargetBounds(grid_item) == target_bounds)
      continue;

    // This is a new grid_item, so do the scale up to identity and fade in
    // animation. The animation is delayed to sync up with the
    // `bounds_animator_` animation.
    if (base::Contains(new_grid_items, grid_item)) {
      grid_item->SetBoundsRect(target_bounds);

      ui::Layer* layer = grid_item->layer();
      layer->SetTransform(GetScaleTransformForView(grid_item));
      layer->SetOpacity(0.f);

      views::AnimationBuilder()
          .Once()
          .Offset(kBoundsChangeAnimationDuration -
                  kTemplateViewsScaleAndFadeDuration)
          .SetTransform(layer, kEndTransform)
          .SetOpacity(layer, 1.f)
          .SetDuration(kTemplateViewsScaleAndFadeDuration);
      continue;
    }

    bounds_animator_.AnimateViewTo(grid_item, target_bounds);
  }

  if (feedback_button_) {
    const gfx::Rect feedback_target_bounds(CalculateFeedbackButtonPosition());
    if (bounds_animator_.GetTargetBounds(feedback_button_) !=
        feedback_target_bounds) {
      bounds_animator_.AnimateViewTo(feedback_button_, feedback_target_bounds);
    }
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
