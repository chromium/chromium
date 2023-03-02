// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_item_util.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dot_indicator.h"
#include "ash/style/style_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/pickle.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Delay in milliseconds of when the dragging UI should be shown for mouse drag.
constexpr int kMouseDragUIDelayInMs = 200;

// Delay in milliseconds of when the dragging UI should be shown for touch drag.
// Note: For better user experience, this is made shorter than
// ET_GESTURE_LONG_PRESS delay, which is too long for this case, e.g., about
// 650ms.
constexpr int kTouchLongpressDelayInMs = 300;

// The drag and drop app icon should get scaled by this factor.
constexpr float kDragDropAppIconScale = 1.2f;

// The drag and drop icon scaling up or down animation transition duration.
constexpr int kDragDropAppIconScaleTransitionInMs = 200;

// The size of the notification indicator circle over the size of the icon.
constexpr float kNotificationIndicatorWidthRatio = 14.0f / 64.0f;

// The size of the notification indicator circle padding over the size of the
// icon.
constexpr float kNotificationIndicatorPaddingRatio = 4.0f / 64.0f;

// Size of the "new install" blue dot that appears to the left of the title.
constexpr int kNewInstallDotSize = 8;

// Distance between the "new install" blue dot and the title.
constexpr int kNewInstallDotPadding = 4;

// The class clips the provided folder icon image.
class ClippedFolderIconImageSource : public gfx::CanvasImageSource {
 public:
  ClippedFolderIconImageSource(const gfx::Size& size,
                               const gfx::ImageSkia& image)
      : gfx::CanvasImageSource(size), image_(image) {}

  ClippedFolderIconImageSource(const ClippedFolderIconImageSource&) = delete;
  ClippedFolderIconImageSource& operator=(const ClippedFolderIconImageSource&) =
      delete;

  ~ClippedFolderIconImageSource() override = default;

  void Draw(gfx::Canvas* canvas) override {
    // Draw the unclipped icon on the center of the canvas with a circular mask.
    SkPath circular_mask;
    circular_mask.addCircle(SkFloatToScalar(size_.width() / 2),
                            SkFloatToScalar(size_.height() / 2),
                            SkIntToScalar(size_.width() / 2));

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    canvas->DrawImageInPath(image_, (size_.width() - image_.size().width()) / 2,
                            (size_.height() - image_.size().height()) / 2,
                            circular_mask, flags);
  }

 private:
  const gfx::ImageSkia image_;
};

// Draws a dot with no shadow.
class DotView : public views::View {
 public:
  DotView() {
    // The dot is not clickable.
    SetCanProcessEventsWithinSubtree(false);
  }
  DotView(const DotView&) = delete;
  DotView& operator=(const DotView&) = delete;
  ~DotView() override = default;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    DCHECK_EQ(width(), height());
    const float radius = width() / 2.0f;
    const float scale = canvas->UndoDeviceScaleFactor();
    gfx::PointF center = gfx::RectF(GetLocalBounds()).CenterPoint();
    center.Scale(scale);

    cc::PaintFlags flags;
    flags.setColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconColorProminent));
    flags.setAntiAlias(true);
    canvas->DrawCircle(center, scale * radius, flags);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }
};

// Returns whether the `index` is considered on the left edge of a grid with
// `cols` columns.
bool IsIndexOnLeftEdge(GridIndex index, int cols) {
  return (index.slot % cols) == 0;
}

// Returns whether the `index` is considered on the right edge of a grid with
// `cols` columns.
bool IsIndexOnRightEdge(GridIndex index, int cols) {
  return ((index.slot + 1) % cols) == 0;
}

bool IsIndexMovingFromOneEdgeToAnother(GridIndex old_index,
                                       GridIndex new_index,
                                       int cols) {
  return (IsIndexOnLeftEdge(new_index, cols) &&
          IsIndexOnRightEdge(old_index, cols)) ||
         (IsIndexOnLeftEdge(old_index, cols) &&
          IsIndexOnRightEdge(new_index, cols));
}

bool IsIndexMovingToDifferentRow(GridIndex old_index,
                                 GridIndex new_index,
                                 int cols) {
  return old_index.slot / cols != new_index.slot / cols ||
         old_index.page != new_index.page;
}

}  // namespace

AppListItemView::AppListItemView(const AppListConfig* app_list_config,
                                 GridDelegate* grid_delegate,
                                 AppListItem* item,
                                 AppListViewDelegate* view_delegate,
                                 Context context)
    : views::Button(
          base::BindRepeating(&GridDelegate::OnAppListItemViewActivated,
                              base::Unretained(grid_delegate),
                              base::Unretained(this))),
      app_list_config_(app_list_config),
      is_folder_(item->GetItemType() == AppListFolderItem::kItemType),
      item_weak_(item),
      grid_delegate_(grid_delegate),
      view_delegate_(view_delegate),
      context_(context) {
  DCHECK(app_list_config_);
  DCHECK(grid_delegate_);
  DCHECK(view_delegate_);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  GetViewAccessibility().OverrideIsLeaf(true);

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false,
                                   /*background_color=*/gfx::kPlaceholderColor);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);

  SetHideInkDropWhenShowingContextMenu(false);
  SetShowInkDropWhenHotTracked(false);
  SetHasInkDropActionOnClick(false);

  StyleUtil::SetUpFocusRingForView(this);
  views::FocusRing::Get(this)->SetHasFocusPredicate([&](View* view) -> bool {
    // With a `view_delegate_` present, focus ring should only show when
    // button is focused and keyboard traversal is engaged.
    if (view_delegate_ && !view_delegate_->KeyboardTraversalEngaged()) {
      return false;
    }

    if (drag_state_ != DragState::kNone) {
      return false;
    }

    if (waiting_for_context_menu_options_ || IsShowingAppMenu()) {
      return false;
    }

    return view->HasFocus();
  });

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(1), app_list_config_->grid_focus_corner_radius());

  auto title = std::make_unique<views::Label>();
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHandlesTooltips(false);
  title->SetFontList(app_list_config_->app_title_font());
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title->SetEnabledColorId(kColorAshTextColorPrimary);

  icon_ = AddChildView(std::make_unique<views::ImageView>());
  icon_->SetCanProcessEventsWithinSubtree(false);
  icon_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

  if (is_folder_) {
    if (features::IsAppCollectionFolderRefreshEnabled()) {
      // Draw the background as part of the icon view.
      EnsureIconBackgroundLayer();
    } else {
      icon_->SetPaintToLayer();
      icon_->layer()->SetFillsBoundsOpaquely(false);
      icon_->SetBackground(views::CreateThemedSolidBackground(
          kColorAshControlBackgroundColorInactive));
    }
    // Set background blur for folder icon and use mask layer to clip it into
    // circle. Note that blur is only enabled in tablet mode to improve dragging
    // smoothness.
    if (view_delegate_->IsInTabletMode()) {
      SetBackgroundBlurEnabled(true);
    }
    SetBackgroundExtendedState(is_icon_extended_, /*animate=*/false);
  }

  notification_indicator_ =
      AddChildView(std::make_unique<DotIndicator>(gfx::kPlaceholderColor));
  notification_indicator_->SetVisible(item->has_notification_badge());

  title_ = AddChildView(std::move(title));

  new_install_dot_ = AddChildView(std::make_unique<DotView>());
  new_install_dot_->SetVisible(item_weak_->is_new_install());

  SetIcon(item_weak_->GetIcon(app_list_config_->type()));
  SetItemName(base::UTF8ToUTF16(item->GetDisplayName()),
              base::UTF8ToUTF16(item->name()));
  item->AddObserver(this);

  if (is_folder_) {
    context_menu_for_folder_ = std::make_unique<AppsGridContextMenu>();
    set_context_menu_controller(context_menu_for_folder_.get());
  } else {
    set_context_menu_controller(this);
  }

  SetAnimationDuration(base::TimeDelta());

  preview_circle_radius_ = 0;
}

void AppListItemView::InitializeIconLoader() {
  DCHECK(item_weak_);
  // Creates app icon load helper. base::Unretained is safe because `this` owns
  // `icon_load_helper_` and `view_delegate_` outlives `this`.
  if (is_folder_) {
    AppListFolderItem* folder_item =
        static_cast<AppListFolderItem*>(item_weak_);
    icon_load_helper_.emplace(
        folder_item->item_list(),
        base::BindRepeating(&AppListViewDelegate::LoadIcon,
                            base::Unretained(view_delegate_)));
  } else {
    icon_load_helper_.emplace(
        item_weak_, base::BindRepeating(&AppListViewDelegate::LoadIcon,
                                        base::Unretained(view_delegate_)));
  }
}

AppListItemView::~AppListItemView() {
  if (item_weak_) {
    item_weak_->RemoveObserver(this);
  }
  StopObservingImplicitAnimations();
}

void AppListItemView::SetIcon(const gfx::ImageSkia& icon) {
  // Clear icon and bail out if item icon is empty.
  if (icon.isNull()) {
    icon_->SetImage(nullptr);
    icon_image_ = gfx::ImageSkia();
    return;
  }
  icon_image_ = icon;

  gfx::Size icon_bounds = is_folder_ ? app_list_config_->unclipped_icon_size()
                                     : app_list_config_->grid_icon_size();

  icon_bounds = gfx::ScaleToRoundedSize(icon_bounds, icon_scale_);

  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST, icon_bounds);
  icon_->SetImage(resized);

  Layout();
}

void AppListItemView::UpdateAppListConfig(
    const AppListConfig* app_list_config) {
  app_list_config_ = app_list_config;

  DCHECK(app_list_config_);

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(1), app_list_config_->grid_focus_corner_radius());

  if (!item_weak_) {
    SetIcon(gfx::ImageSkia());
    return;
  }

  title()->SetFontList(app_list_config_->app_title_font());
  SetIcon(item_weak_->GetIcon(app_list_config_->type()));
  SetBackgroundExtendedState(is_icon_extended_, /*animate=*/false);
  SchedulePaint();
}

void AppListItemView::ScaleIconImmediatly(float scale_factor) {
  if (icon_scale_ == scale_factor) {
    return;
  }
  icon_scale_ = scale_factor;
  SetIcon(icon_image_);
  layer()->SetTransform(gfx::Transform());
}

void AppListItemView::UpdateBackgroundLayerBounds() {
  auto* background_layer = GetIconBackgroundLayer();
  if (!background_layer || !features::IsAppCollectionFolderRefreshEnabled() ||
      icon_->bounds().IsEmpty()) {
    return;
  }

  if (is_folder_) {
    // The folder icon already has the same size as its background layer.
    background_layer->SetBounds(icon_->layer()->bounds());
    return;
  }

  // Set the background layer size of the app icon to `unclipped_icon_dimension`
  // for the clip rect animation.
  gfx::Rect background_bounds = icon_->layer()->bounds();
  int outset_from_icon =
      (app_list_config_->unclipped_icon_dimension() * icon_scale_ -
       background_bounds.width()) /
      2;
  background_bounds.Outset(outset_from_icon);
  background_layer->SetBounds(background_bounds);

  // Note that the background size should initially be the folder icon size
  // instead of the grid icon size. This is because the app icon has a
  // transparent ring around the visible icon which makes it look smaller.
  background_bounds.ClampToCenteredSize(gfx::ScaleToRoundedSize(
      app_list_config_->icon_visible_size(), icon_scale_));
  background_layer->SetRoundedCornerRadius(
      gfx::RoundedCornersF(background_bounds.width() / 2));
}

void AppListItemView::SetUIState(UIState ui_state) {
  if (ui_state_ == ui_state) {
    return;
  }

  switch (ui_state) {
    case UI_STATE_NORMAL:
      title_->SetVisible(true);
      if (item_weak_) {
        ItemIsNewInstallChanged();
      }
      if (ui_state_ == UI_STATE_DRAGGING) {
        GetWidget()->SetCursor(ui::mojom::CursorType::kNull);
        ScaleAppIcon(false);
      }
      break;
    case UI_STATE_DRAGGING:
      title_->SetVisible(false);
      if (new_install_dot_) {
        new_install_dot_->SetVisible(false);
      }
      if (ui_state_ == UI_STATE_NORMAL && !in_cardified_grid_) {
        GetWidget()->SetCursor(ui::mojom::CursorType::kGrabbing);
        ScaleAppIcon(true);
      }
      break;
    case UI_STATE_DROPPING_IN_FOLDER:
      break;
  }
  ui_state_ = ui_state;

  SchedulePaint();
}

void AppListItemView::ScaleAppIcon(bool scale_up) {
  if (!layer()) {
    return;
  }
  if (!is_folder_) {
    if (scale_up) {
      icon_scale_ = kDragDropAppIconScale;
      SetIcon(icon_image_);
      layer()->SetTransform(gfx::GetScaleTransform(
          GetContentsBounds().CenterPoint(), 1 / kDragDropAppIconScale));
    } else if (drag_state_ != DragState::kNone) {
      // If a drag view has been created for this icon, the item transition to
      // target bounds is handled by the apps grid view bounds animator. At the
      // end of that animation, the layer will be destroyed, causing the
      // animation observer to get canceled. For this case, we need to scale
      // down the icon immediately, with no animation.
      ScaleIconImmediatly(1.0f);
    }
  }

  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::Milliseconds((kDragDropAppIconScaleTransitionInMs)));
  settings.SetTweenType(gfx::Tween::EASE_OUT_2);
  if (scale_up) {
    if (is_folder_) {
      const gfx::Rect bounds(layer()->bounds().size());
      gfx::Transform transform =
          gfx::GetScaleTransform(bounds.CenterPoint(), kDragDropAppIconScale);
      layer()->SetTransform(transform);
    } else {
      layer()->SetTransform(gfx::Transform());
    }
  } else {
    if (is_folder_) {
      layer()->SetTransform(gfx::Transform());
    } else if (drag_state_ == DragState::kNone) {
      // To avoid poor quality icons, update icon image with the correct scale
      // after the transform animation is completed.
      settings.AddObserver(this);
      layer()->SetTransform(gfx::GetScaleTransform(
          GetContentsBounds().CenterPoint(), 1 / kDragDropAppIconScale));
    }
  }
}

void AppListItemView::OnImplicitAnimationsCompleted() {
  ScaleIconImmediatly(1.0f);
}

void AppListItemView::SetTouchDragging(bool touch_dragging) {
  // Drag and drop refactor handles all drag operations as Mouse Dragging.
  // TODO(b/261985897): Figure out a way to correctly direct drag operations.
  DCHECK(!app_list_features::IsDragAndDropRefactorEnabled());

  if (touch_dragging_ == touch_dragging) {
    return;
  }

  touch_dragging_ = touch_dragging;

  if (context_menu_for_folder_)
    context_menu_for_folder_->set_owner_touch_dragging(touch_dragging_);

  SetState(STATE_NORMAL);
  SetUIState(touch_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);

  // EndDrag may delete |this|.
  if (!touch_dragging) {
    grid_delegate_->EndDrag(/*cancel=*/false);
  }
}

void AppListItemView::SetMouseDragging(bool mouse_dragging) {
  mouse_dragging_ = mouse_dragging;

  SetState(STATE_NORMAL);
  SetUIState(mouse_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);
}

void AppListItemView::OnMouseDragTimer() {
  DCHECK(!app_list_features::IsDragAndDropRefactorEnabled());
  // Show scaled up app icon to indicate draggable state.
  SetMouseDragging(true);
}

void AppListItemView::OnTouchDragTimer(
    const gfx::Point& tap_down_location,
    const gfx::Point& tap_down_root_location) {
  DCHECK(!app_list_features::IsDragAndDropRefactorEnabled());
  // Show scaled up app icon to indicate draggable state.
  if (!InitiateDrag(tap_down_location, tap_down_root_location)) {
    return;
  }

  SetTouchDragging(true);
}

bool AppListItemView::InitiateDrag(const gfx::Point& location,
                                   const gfx::Point& root_location) {
  DCHECK(!app_list_features::IsDragAndDropRefactorEnabled());

  if (!grid_delegate_->InitiateDrag(
          this, location, root_location,
          base::BindOnce(&AppListItemView::OnDragStarted,
                         weak_ptr_factory_.GetWeakPtr()),
          base::BindOnce(&AppListItemView::OnDragEnded,
                         weak_ptr_factory_.GetWeakPtr()))) {
    return false;
  }
  drag_state_ = DragState::kInitialized;
  return true;
}

void AppListItemView::OnDragStarted() {
  mouse_drag_timer_.Stop();
  touch_drag_timer_.Stop();
  drag_state_ = DragState::kStarted;
  SetUIState(UI_STATE_DRAGGING);
  CancelContextMenu();
}

void AppListItemView::OnDragEnded() {
  mouse_dragging_ = false;
  mouse_drag_timer_.Stop();

  touch_dragging_ = false;
  touch_drag_timer_.Stop();

  if (context_menu_for_folder_)
    context_menu_for_folder_->set_owner_touch_dragging(false);

  SetUIState(UI_STATE_NORMAL);
  drag_state_ = DragState::kNone;
}

void AppListItemView::CancelContextMenu() {
  if (item_menu_model_adapter_) {
    menu_close_initiated_from_drag_ = true;
    item_menu_model_adapter_->Cancel();
  }
  if (context_menu_for_folder_) {
    context_menu_for_folder_->Cancel();
  }
}

gfx::Point AppListItemView::GetDragImageOffset() {
  gfx::Point image = icon_->GetImageBounds().origin();
  return gfx::Point(icon_->x() + image.x(), icon_->y() + image.y());
}

void AppListItemView::SetAsAttemptedFolderTarget(bool is_target_folder) {
  if (is_target_folder)
    SetUIState(UI_STATE_DROPPING_IN_FOLDER);
  else
    SetUIState(UI_STATE_NORMAL);
}

void AppListItemView::SilentlyRequestFocus() {
  DCHECK(!focus_silently_);
  base::AutoReset<bool> auto_reset(&focus_silently_, true);
  RequestFocus();
}

void AppListItemView::EnsureSelected() {
  grid_delegate_->SetSelectedView(/*view=*/this);
}

void AppListItemView::SetItemName(const std::u16string& display_name,
                                  const std::u16string& full_name) {
  const std::u16string folder_name_placeholder =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER);
  if (is_folder_ && display_name.empty()) {
    title_->SetText(folder_name_placeholder);
  } else {
    title_->SetText(display_name);
  }

  tooltip_text_ = display_name == full_name ? std::u16string() : full_name;

  // Use full name for accessibility.
  SetAccessibleName(
      is_folder_ ? l10n_util::GetStringFUTF16(
                       IDS_APP_LIST_FOLDER_BUTTON_ACCESSIBILE_NAME,
                       full_name.empty() ? folder_name_placeholder : full_name)
                 : full_name);
  Layout();
}

void AppListItemView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // When this item is being removed, there will still be an accessible object
  // in the accessibility tree until it is destroyed. Populating AXNodeData
  // with the information from the button makes it possible for assistive
  // technologies to obtain the name and role/type of the control along with
  // relevant states such as disabled. It is also necessary to pass the
  // accessibility paint checks: items that claim to be focusable must have
  // a valid role.
  DCHECK(node_data);
  Button::GetAccessibleNodeData(node_data);

  if (!item_weak_) {
    return;
  }

  auto app_status = item_weak_->app_status();
  switch (app_status) {
    case AppStatus::kBlocked:
      node_data->SetDescription(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_BLOCKED_APP));
      break;
    case AppStatus::kPaused:
      node_data->SetDescription(
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_PAUSED_APP));
      break;
    default:
      if (item_weak_->is_new_install()) {
        node_data->SetDescription(
            ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                IDS_APP_LIST_NEW_INSTALL_ACCESSIBILE_DESCRIPTION));
      }
      break;
  }
}

void AppListItemView::OnContextMenuModelReceived(
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  waiting_for_context_menu_options_ = false;
  if (!menu_model || IsShowingAppMenu()) {
    return;
  }

  // GetContextMenuModel is asynchronous and takes a nontrivial amount of time
  // to complete. If a menu is shown after the icon has moved, |grid_delegate_|
  // gets put in a bad state because the context menu begins to receive drag
  // events, interrupting the app icon drag.
  if (drag_state_ == DragState::kStarted) {
    return;
  }

  menu_show_initiated_from_key_ = source_type == ui::MENU_SOURCE_KEYBOARD;

  // Clear the existing focus in other elements to prevent having a focus
  // indicator on other non-selected views.
  if (GetFocusManager()->GetFocusedView()) {
    GetFocusManager()->ClearFocus();
    focus_removed_by_context_menu_ = true;
  }

  if (!grid_delegate_->IsSelectedView(this))
    grid_delegate_->ClearSelectedView();

  int run_types = views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::USE_ASH_SYS_UI_LAYOUT |
                  views::MenuRunner::FIXED_ANCHOR |
                  views::MenuRunner::CONTEXT_MENU;

  if (source_type == ui::MENU_SOURCE_TOUCH && touch_dragging_)
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;

  // Screen bounds don't need RTL flipping.
  gfx::Rect anchor_rect = GetBoundsInScreen();

  // Assign the correct app type to `item_menu_model_adapter_` according to the
  // parent view of the app list item view.
  AppListMenuModelAdapter::AppListViewAppType app_type;
  AppLaunchedMetricParams metric_params;
  switch (context_) {
    case Context::kAppsGridView:
      app_type = AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_APP_GRID;
      metric_params.launched_from = AppListLaunchedFrom::kLaunchedFromGrid;
      metric_params.launch_type = AppListLaunchType::kApp;
      break;
    case Context::kRecentAppsView:
      app_type = AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_RECENT_APP;
      metric_params.launched_from =
          AppListLaunchedFrom::kLaunchedFromRecentApps;
      metric_params.launch_type = AppListLaunchType::kAppSearchResult;
      break;
  }
  view_delegate_->GetAppLaunchedMetricParams(&metric_params);

  item_menu_model_adapter_ = std::make_unique<AppListMenuModelAdapter>(
      item_weak_->GetMetadata()->id, std::move(menu_model), GetWidget(),
      source_type, metric_params, app_type,
      base::BindOnce(&AppListItemView::OnMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      view_delegate_->IsInTabletMode());

  item_menu_model_adapter_->Run(
      anchor_rect, views::MenuAnchorPosition::kBubbleRight, run_types);

  if (!context_menu_shown_callback_.is_null()) {
    context_menu_shown_callback_.Run();
  }

  grid_delegate_->SetSelectedView(this);
}

void AppListItemView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (IsShowingAppMenu()) {
    return;
  }
  // Prevent multiple requests for context menus before the current request
  // completes. If a second request is sent before the first one can respond,
  // the Chrome side delegate will become unresponsive
  // (https://crbug.com/881886).
  if (waiting_for_context_menu_options_) {
    return;
  }
  waiting_for_context_menu_options_ = true;
  views::InkDrop::Get(this)->SetMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::ACTIVATED,
                                            nullptr);

  // When the context menu comes from the apps grid it has sorting options. When
  // it comes from recent apps it has an option to hide the continue section.
  AppListItemContext item_context = context_ == Context::kAppsGridView
                                        ? AppListItemContext::kAppsGrid
                                        : AppListItemContext::kRecentApps;
  view_delegate_->GetContextMenuModel(
      item_weak_->id(), item_context,
      base::BindOnce(&AppListItemView::OnContextMenuModelReceived,
                     weak_ptr_factory_.GetWeakPtr(), point, source_type));
}

bool AppListItemView::ShouldEnterPushedState(const ui::Event& event) {
  if (drag_state_ != DragState::kNone) {
    return false;
  }
  // Don't enter pushed state for ET_GESTURE_TAP_DOWN so that hover gray
  // background does not show up during scroll.
  if (event.type() == ui::ET_GESTURE_TAP_DOWN) {
    return false;
  }

  return views::Button::ShouldEnterPushedState(event);
}

bool AppListItemView::OnMousePressed(const ui::MouseEvent& event) {
  bool return_value = Button::OnMousePressed(event);

  if (app_list_features::IsDragAndDropRefactorEnabled()) {
    return return_value;
  }

  if (!ShouldEnterPushedState(event)) {
    return true;
  }

  if (!InitiateDrag(event.location(), event.root_location())) {
    return true;
  }

  mouse_drag_timer_.Start(FROM_HERE, base::Milliseconds(kMouseDragUIDelayInMs),
                          this, &AppListItemView::OnMouseDragTimer);
  return true;
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    return;
  }

  views::FocusRing::Get(this)->Layout();

  const gfx::Rect icon_bounds = GetIconBoundsForTargetViewBounds(
      app_list_config_, rect, icon_->GetImageBounds().size(), icon_scale_);
  icon_->SetBoundsRect(icon_bounds);
  UpdateBackgroundLayerBounds();
  SetBackgroundExtendedState(is_icon_extended_, /*animate=*/false);

  gfx::Rect title_bounds = GetTitleBoundsForTargetViewBounds(
      app_list_config_, rect, title_->GetPreferredSize(), icon_scale_);
  if (new_install_dot_ && new_install_dot_->GetVisible()) {
    // If the new install dot is showing, and the dot would extend outside the
    // left edge of the tile, inset the title bounds to make space for the dot.
    int dot_x = title_bounds.x() - kNewInstallDotSize - kNewInstallDotPadding;
    if (dot_x < 0)
      title_bounds.Inset(gfx::Insets::TLBR(0, kNewInstallDotSize, 0, 0));
  }
  title_->SetBoundsRect(title_bounds);

  if (new_install_dot_) {
    new_install_dot_->SetBounds(
        title_bounds.x() - kNewInstallDotSize - kNewInstallDotPadding,
        title_bounds.y() + title_bounds.height() / 2 - kNewInstallDotSize / 2,
        kNewInstallDotSize, kNewInstallDotSize);
  }

  const float indicator_size =
      icon_bounds.width() * kNotificationIndicatorWidthRatio;
  const float indicator_padding =
      icon_bounds.width() * kNotificationIndicatorPaddingRatio;

  const float indicator_x =
      icon_bounds.right() - indicator_size - indicator_padding;
  const float indicator_y = icon_bounds.y() + indicator_padding;

  const gfx::Rect indicator_bounds = gfx::ToRoundedRect(
      gfx::RectF(indicator_x, indicator_y, indicator_size, indicator_size));
  notification_indicator_->SetIndicatorBounds(indicator_bounds);
}

gfx::Size AppListItemView::CalculatePreferredSize() const {
  return gfx::Size(app_list_config_->grid_tile_width(),
                   app_list_config_->grid_tile_height());
}

bool AppListItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Disable space key to press the button. The keyboard events received
  // by this view are forwarded from a Textfield (SearchBoxView) and key
  // released events are not forwarded. This leaves the button in pressed
  // state.
  if (event.key_code() == ui::VKEY_SPACE) {
    return false;
  }

  return Button::OnKeyPressed(event);
}

void AppListItemView::OnMouseReleased(const ui::MouseEvent& event) {
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  // Triggers the button's click handler callback, which might delete `this`.
  Button::OnMouseReleased(event);
  if (!weak_this) {
    return;
  }

  if (app_list_features::IsDragAndDropRefactorEnabled()) {
    return;
  }

  SetMouseDragging(false);

  // EndDrag may delete |this|.
  grid_delegate_->EndDrag(/*cancel=*/false);
}

void AppListItemView::OnMouseCaptureLost() {
  Button::OnMouseCaptureLost();
  SetMouseDragging(false);

  if (app_list_features::IsDragAndDropRefactorEnabled()) {
    return;
  }

  // EndDrag may delete |this|.
  grid_delegate_->EndDrag(/*cancel=*/true);
}

bool AppListItemView::OnMouseDragged(const ui::MouseEvent& event) {
  bool return_value = Button::OnMouseDragged(event);

  if (app_list_features::IsDragAndDropRefactorEnabled()) {
    return return_value;
  }

  if (drag_state_ != DragState::kNone && mouse_dragging_) {
    // Update the drag location of the drag proxy if it has been created.
    // If the drag is no longer happening, it could be because this item
    // got removed, in which case this item has been destroyed. So, bail out
    // now as there will be nothing else to do anyway as
    // grid_delegate_->IsDragging() will be false.
    if (!grid_delegate_->UpdateDragFromItem(/*is_touch=*/false, event))
      return true;
  }

  if (!grid_delegate_->IsSelectedView(this))
    grid_delegate_->ClearSelectedView();
  return true;
}

bool AppListItemView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

void AppListItemView::OnFocus() {
  if (focus_silently_) {
    return;
  }
  grid_delegate_->SetSelectedView(this);
  views::FocusRing::Get(this)->SchedulePaint();
}

void AppListItemView::OnBlur() {
  if (grid_delegate_->IsSelectedView(this)) {
    grid_delegate_->ClearSelectedView();
  }
  views::FocusRing::Get(this)->SchedulePaint();
}

int AppListItemView::GetDragOperations(const gfx::Point& press_pt) {
  return app_list_features::IsDragAndDropRefactorEnabled()
             ? ui::DragDropTypes::DRAG_MOVE
             : views::View::GetDragOperations(press_pt);
}

void AppListItemView::WriteDragData(const gfx::Point& press_pt,
                                    OSExchangeData* data) {
  if (!app_list_features::IsDragAndDropRefactorEnabled()) {
    views::View::WriteDragData(press_pt, data);
    return;
  }

  SetMouseDragging(true);
  if (item_weak_) {
    data->provider().SetDragImage(icon_image_, press_pt.OffsetFromOrigin());
    base::Pickle data_pickle;
    data_pickle.WriteString(item_weak_->id());
    data->SetPickledData(GetAppItemFormatType(), data_pickle);
  }
}

void AppListItemView::OnGestureEvent(ui::GestureEvent* event) {
  if (app_list_features::IsDragAndDropRefactorEnabled() &&
      event->type() != ui::ET_GESTURE_TAP_DOWN) {
    Button::OnGestureEvent(event);
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (touch_dragging_) {
        grid_delegate_->StartDragAndDropHostDragAfterLongPress();
        event->SetHandled();
      } else {
        touch_drag_timer_.Stop();
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (touch_dragging_ && drag_state_ != DragState::kNone) {
        grid_delegate_->UpdateDragFromItem(/*is_touch=*/true, *event);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
      if (touch_dragging_) {
        SetTouchDragging(false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_TAP_DOWN:
      if (GetState() != STATE_DISABLED) {
        SetState(STATE_PRESSED);
        touch_drag_timer_.Start(
            FROM_HERE, base::Milliseconds(kTouchLongpressDelayInMs),
            base::BindOnce(&AppListItemView::OnTouchDragTimer,
                           base::Unretained(this), event->location(),
                           event->root_location()));
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (GetState() != STATE_DISABLED) {
        touch_drag_timer_.Stop();
        SetState(STATE_NORMAL);
      }
      break;
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_END:
      touch_drag_timer_.Stop();
      SetTouchDragging(false);
      if (IsShowingAppMenu()) {
        grid_delegate_->SetSelectedView(this);
      }
      break;
    case ui::ET_GESTURE_TWO_FINGER_TAP:
      if (touch_dragging_) {
        SetTouchDragging(false);
      } else {
        touch_drag_timer_.Stop();
      }
      break;
    default:
      break;
  }
  if (!event->handled()) {
    Button::OnGestureEvent(event);
  }
}

void AppListItemView::OnThemeChanged() {
  views::Button::OnThemeChanged();
  if (item_weak_) {
    item_weak_->RequestFolderIconUpdate();
    SkColor notification_indicator_color =
        is_folder_ ? GetColorProvider()->GetColor(cros_tokens::kIconColorBlue)
                   : item_weak_->GetNotificationBadgeColor();
    notification_indicator_->SetColor(notification_indicator_color);
    if (icon_background_layer_.OwnsLayer()) {
      icon_background_layer_.layer()->SetColor(GetColorProvider()->GetColor(
          kColorAshControlBackgroundColorInactive));
    }
  }
  SchedulePaint();
}

std::u16string AppListItemView::GetTooltipText(const gfx::Point& p) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
  title_->SetTooltipText(tooltip_text_);
  std::u16string tooltip = title_->GetTooltipText(p);
  title_->SetHandlesTooltips(false);
  if (new_install_dot_ && new_install_dot_->GetVisible() && !is_folder_) {
    // Tooltip becomes two lines: "App Name" + "New install".
    tooltip = l10n_util::GetStringFUTF16(IDS_APP_LIST_NEW_INSTALL, tooltip);
  }
  return tooltip;
}

void AppListItemView::OnDraggedViewEnter() {
  SetBackgroundExtendedState(/*extend_icon=*/true, /*animate=*/true);
}

void AppListItemView::OnDraggedViewExit() {
  SetBackgroundExtendedState(/*extend_icon=*/false, /*animate=*/true);
}

void AppListItemView::SetBackgroundBlurEnabled(bool enabled) {
  DCHECK(is_folder_);
  if (!enabled) {
    if (GetIconBackgroundLayer()) {
      GetIconBackgroundLayer()->SetBackgroundBlur(0);
    }
    return;
  }
  GetIconBackgroundLayer()->SetBackgroundBlur(
      ColorProvider::kBackgroundBlurSigma);
  GetIconBackgroundLayer()->SetBackdropFilterQuality(
      ColorProvider::kBackgroundBlurQuality);
}

void AppListItemView::EnsureLayer() {
  if (layer()) {
    return;
  }
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

bool AppListItemView::HasNotificationBadge() {
  return item_weak_->has_notification_badge();
}

void AppListItemView::FireMouseDragTimerForTest() {
  mouse_drag_timer_.FireNow();
}

bool AppListItemView::FireTouchDragTimerForTest() {
  if (!touch_drag_timer_.IsRunning()) {
    return false;
  }

  touch_drag_timer_.FireNow();
  return true;
}

bool AppListItemView::IsShowingAppMenu() const {
  return item_menu_model_adapter_ && item_menu_model_adapter_->IsShowingMenu();
}

bool AppListItemView::IsNotificationIndicatorShownForTest() const {
  return notification_indicator_->GetVisible();
}

void AppListItemView::SetContextMenuShownCallbackForTest(
    base::RepeatingClosure closure) {
  context_menu_shown_callback_ = std::move(closure);
}

gfx::Rect AppListItemView::GetDefaultTitleBoundsForTest() {
  return GetTitleBoundsForTargetViewBounds(
      app_list_config_, GetContentsBounds(), title_->GetPreferredSize(),
      icon_scale_);
}

void AppListItemView::SetMostRecentGridIndex(GridIndex new_grid_index,
                                             int columns) {
  if (new_grid_index == most_recent_grid_index_) {
    has_pending_row_change_ = false;
    return;
  }

  if (most_recent_grid_index_.IsValid()) {
    // Pending row changes are only flagged when the item index changes from one
    // edge of the grid to the other and into a different row.
    if (IsIndexMovingFromOneEdgeToAnother(most_recent_grid_index_,
                                          new_grid_index, columns) &&
        IsIndexMovingToDifferentRow(most_recent_grid_index_, new_grid_index,
                                    columns)) {
      has_pending_row_change_ = true;
    } else {
      has_pending_row_change_ = false;
    }
  }

  most_recent_grid_index_ = new_grid_index;
}

void AppListItemView::OnMenuClosed() {
  views::InkDrop::Get(this)->AnimateToState(views::InkDropState::HIDDEN,
                                            nullptr);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);

  // Release menu since its menu model delegate (AppContextMenu) could be
  // released as a result of menu command execution.
  item_menu_model_adapter_.reset();

  if (!menu_close_initiated_from_drag_) {
    // If the menu was not closed due to a drag sequence(e.g. multi touch) reset
    // the drag state.
    SetState(STATE_NORMAL);
    SetTouchDragging(false);
  }

  menu_close_initiated_from_drag_ = false;

  // Keep the item focused if the menu was shown via keyboard.
  if (!menu_show_initiated_from_key_) {
    OnBlur();
  }

  if (focus_removed_by_context_menu_) {
    // Restore the last focused view when exiting the menu.
    GetFocusManager()->RestoreFocusedView();
    focus_removed_by_context_menu_ = false;
  }
}

void AppListItemView::OnSyncDragEnd() {
  SetUIState(UI_STATE_NORMAL);
}

gfx::Rect AppListItemView::GetIconBounds() const {
  if (is_folder_) {
    // The folder icon is in unclipped size, so clip it before return.
    gfx::Rect folder_icon_bounds = icon_->bounds();
    folder_icon_bounds.ClampToCenteredSize(
        app_list_config_->icon_visible_size());
    return folder_icon_bounds;
  }
  return icon_->bounds();
}

gfx::Rect AppListItemView::GetIconBoundsInScreen() const {
  gfx::Rect icon_bounds = GetIconBounds();
  ConvertRectToScreen(this, &icon_bounds);
  return icon_bounds;
}

gfx::ImageSkia AppListItemView::GetIconImage() const {
  if (!is_folder_) {
    return icon_->GetImage();
  }

  return gfx::CanvasImageSource::MakeImageSkia<ClippedFolderIconImageSource>(
      app_list_config_->icon_visible_size(), icon_->GetImage());
}

void AppListItemView::SetIconVisible(bool visible) {
  icon_->SetVisible(visible);
}

void AppListItemView::EnterCardifyState() {
  in_cardified_grid_ = true;
  gfx::FontList font_size = app_list_config_->app_title_font();
  const float cardified_scale = GetAppsGridCardifiedScale();
  const int size_delta = font_size.GetFontSize() * (1 - cardified_scale);
  title_->SetFontList(font_size.DeriveWithSizeDelta(-size_delta));
  ScaleIconImmediatly(cardified_scale);
}

void AppListItemView::ExitCardifyState() {
  title_->SetFontList(app_list_config_->app_title_font());
  ScaleIconImmediatly(1.0f);
  in_cardified_grid_ = false;
}

// static
gfx::Rect AppListItemView::GetIconBoundsForTargetViewBounds(
    const AppListConfig* config,
    const gfx::Rect& target_bounds,
    const gfx::Size& icon_size,
    const float icon_scale) {
  gfx::Rect rect(target_bounds);
  rect.Inset(gfx::Insets::TLBR(
      0, 0, config->grid_icon_bottom_padding() * icon_scale, 0));
  rect.ClampToCenteredSize(icon_size);
  return rect;
}

// static
gfx::Rect AppListItemView::GetTitleBoundsForTargetViewBounds(
    const AppListConfig* config,
    const gfx::Rect& target_bounds,
    const gfx::Size& title_size,
    float icon_scale) {
  gfx::Rect rect(target_bounds);
  rect.Inset(
      gfx::Insets::TLBR(config->grid_title_top_padding() * icon_scale,
                        config->grid_title_horizontal_padding() * icon_scale,
                        config->grid_title_bottom_padding() * icon_scale,
                        config->grid_title_horizontal_padding() * icon_scale));
  rect.ClampToCenteredSize(title_size);
  // Respect the title preferred height, to ensure the text does not get clipped
  // due to padding if the item view gets too small.
  if (rect.height() < title_size.height()) {
    rect.set_y(rect.y() - (title_size.height() - rect.height()) / 2);
    rect.set_height(title_size.height());
  }
  return rect;
}

void AppListItemView::ItemIconChanged(AppListConfigType config_type) {
  if (config_type != app_list_config_->type()) {
    return;
  }

  DCHECK(item_weak_);
  SetIcon(item_weak_->GetIcon(app_list_config_->type()));
}

void AppListItemView::ItemNameChanged() {
  SetItemName(base::UTF8ToUTF16(item_weak_->GetDisplayName()),
              base::UTF8ToUTF16(item_weak_->name()));
}

void AppListItemView::ItemBadgeVisibilityChanged() {
  if (icon_)
    notification_indicator_->SetVisible(item_weak_->has_notification_badge());
}

void AppListItemView::ItemBadgeColorChanged() {
  notification_indicator_->SetColor(item_weak_->GetNotificationBadgeColor());
}

void AppListItemView::ItemIsNewInstallChanged() {
  DCHECK(item_weak_);
  if (new_install_dot_) {
    new_install_dot_->SetVisible(item_weak_->is_new_install());
    Layout();
  }
}

void AppListItemView::ItemBeingDestroyed() {
  DCHECK(item_weak_);
  item_weak_->RemoveObserver(this);
  item_weak_ = nullptr;

  // TODO(b/261985897): Consider canceling drag when the item is being
  // destroyed.
  if (app_list_features::IsDragAndDropRefactorEnabled()) {
    return;
  }

  // `EndDrag()` may delete this.
  if (drag_state_ != DragState::kNone) {
    grid_delegate_->EndDrag(/*cancel=*/true);
  }
}

void AppListItemView::SetBackgroundExtendedState(bool extend_icon,
                                                 bool animate) {
  // App backgrounds are only created or updated if the extended state changes,
  // while unchanged folders may update the icon clip rects. Return early for
  // unchanged apps.
  if (is_icon_extended_ == extend_icon && !is_folder_) {
    return;
  }

  is_icon_extended_ = extend_icon;
  EnsureIconBackgroundLayer();
  base::AutoReset<bool> auto_reset(&setting_up_icon_animation_, true);
  ui::Layer* background_layer = GetIconBackgroundLayer();
  DCHECK(background_layer);

  views::AnimationBuilder builder;
  const auto animation_tween_type = gfx::Tween::EASE_IN;

  builder
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&AppListItemView::OnExtendingAnimationEnded,
                              weak_ptr_factory_.GetWeakPtr(), extend_icon))
      .OnAborted(base::BindOnce(&AppListItemView::OnExtendingAnimationEnded,
                                weak_ptr_factory_.GetWeakPtr(), extend_icon))
      .Once();

  if (features::IsAppCollectionFolderRefreshEnabled()) {
    UpdateBackgroundLayerBounds();
    const int width = extend_icon ? app_list_config_->unclipped_icon_dimension()
                                  : app_list_config_->icon_visible_dimension();
    gfx::Rect clip_rect(background_layer->size());
    clip_rect.ClampToCenteredSize(
        ScaleToRoundedSize(gfx::Size(width, width), icon_scale_));

    const int corner_radius =
        extend_icon ? app_list_config_->icon_extended_background_radius()
                    : width / 2;
    builder.GetCurrentSequence()
        .SetDuration(base::Milliseconds(animate ? 125 : 0))
        .SetClipRect(background_layer, clip_rect, animation_tween_type)
        .SetRoundedCorners(background_layer,
                           gfx::RoundedCornersF(corner_radius * icon_scale_),
                           animation_tween_type);
    return;
  }

  // Handle folder icons
  if (is_folder_) {
    const int corner_radius =
        extend_icon ? app_list_config_->unclipped_icon_dimension() / 2
                    : app_list_config_->icon_visible_dimension() / 2;

    gfx::Rect clip_rect = icon_->GetLocalBounds();
    if (!extend_icon) {
      clip_rect.Inset(gfx::Insets(app_list_config_->folder_icon_insets()));
    }
    builder.GetCurrentSequence()
        .SetDuration(base::Milliseconds(animate ? 125 : 0))
        .SetClipRect(background_layer, clip_rect, animation_tween_type)
        .SetRoundedCorners(background_layer,
                           gfx::RoundedCornersF(corner_radius),
                           animation_tween_type);
    return;
  }

  // Handle app icons
  gfx::Rect background_target_bounds(icon_->layer()->bounds().CenterPoint(),
                                     gfx::Size());
  if (extend_icon) {
    background_layer->SetBounds(background_target_bounds);
    background_layer->SetColor(
        GetColorProvider()->GetColor(kColorAshControlBackgroundColorInactive));
    background_target_bounds.Outset(
        app_list_config_->folder_dropping_circle_radius() * icon_scale_);
  }
  builder.GetCurrentSequence()
      .SetDuration(base::Milliseconds(animate ? 250 : 0))
      .SetBounds(background_layer, background_target_bounds,
                 animation_tween_type)
      .SetRoundedCorners(
          background_layer,
          gfx::RoundedCornersF(background_target_bounds.width() / 2),
          animation_tween_type);
}

void AppListItemView::EnsureIconBackgroundLayer() {
  const bool clip_inner_icons =
      is_folder_ && !features::IsAppCollectionFolderRefreshEnabled();
  if (clip_inner_icons || icon_background_layer_.OwnsLayer()) {
    return;
  }

  icon_background_layer_.Reset(
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR));
  auto* background_layer = icon_background_layer_.layer();
  background_layer->SetName("icon_background_layer");
  if (GetColorProvider()) {
    background_layer->SetColor(
        GetColorProvider()->GetColor(kColorAshControlBackgroundColorInactive));
  }
  icon_->AddLayerToRegion(background_layer, views::LayerRegion::kBelow);
}

void AppListItemView::OnExtendingAnimationEnded(bool extend_icon) {
  if (!setting_up_icon_animation_ && !extend_icon && !is_folder_) {
    icon_->RemoveLayerFromRegions(icon_background_layer_.layer());
    icon_background_layer_.ReleaseLayer();
  }
}

ui::Layer* AppListItemView::GetIconBackgroundLayer() {
  if (is_folder_ && !features::IsAppCollectionFolderRefreshEnabled()) {
    return icon_->layer();
  }

  return icon_background_layer_.layer();
}

BEGIN_METADATA(AppListItemView, views::Button)
END_METADATA

}  // namespace ash
