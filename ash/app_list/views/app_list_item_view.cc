// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/shadow_value.h"
#include "ui/gfx/transform_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/drag_controller.h"

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

// The color of the title for the tiles within folder.
constexpr SkColor kFolderGridTitleColor = SK_ColorBLACK;

// The color of the focus ring within a folder.
constexpr SkColor kFolderGridFocusRingColor = gfx::kGoogleBlue600;

// The color of an item selected via right-click context menu.
constexpr SkColor kContextSelection = SkColorSetA(gfx::kGoogleGrey100, 31);

// The color of an item selected via right-click context menu in a folder.
constexpr SkColor kContextSelectionFolder =
    SkColorSetA(gfx::kGoogleGrey900, 21);

// The width of the focus ring within a folder.
constexpr int kFocusRingWidth = 2;

// The shadow blur of title.
constexpr int kTitleShadowBlur = 28;

// The shadow color of title.
constexpr SkColor kTitleShadowColor = SkColorSetA(SK_ColorBLACK, 82);

// The shadow blur of icon.
constexpr int kIconShadowBlur = 10;

// The shadow color of icon.
constexpr SkColor kIconShadowColor = SkColorSetA(SK_ColorBLACK, 31);

// The class clips the provided folder icon image.
class ClippedFolderIconImageSource : public gfx::CanvasImageSource {
 public:
  ClippedFolderIconImageSource(const gfx::Size& size,
                               const gfx::ImageSkia& image)
      : gfx::CanvasImageSource(size), image_(image) {}
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

  DISALLOW_COPY_AND_ASSIGN(ClippedFolderIconImageSource);
};

}  // namespace

// ImageView for the item icon.
class AppListItemView::IconImageView : public views::ImageView {
 public:
  IconImageView() {
    set_can_process_events_within_subtree(false);
    SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  }
  ~IconImageView() override = default;

  // views::View:
  const char* GetClassName() const override {
    return "AppListItemView::IconImageView";
  }
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override {
    views::ImageView::OnBoundsChanged(previous_bounds);
    if (size() != previous_bounds.size() && !insets_.IsEmpty())
      SetRoundedCornerAndInsets(corner_radius_, insets_);
  }

  // ui::LayerOwner:
  std::unique_ptr<ui::Layer> RecreateLayer() override {
    std::unique_ptr<ui::Layer> old_layer = views::View::RecreateLayer();

    // ui::Layer::Clone() does not copy the clip rect, so set it explicitly
    // here.
    if (corner_radius_ != 0 || !insets_.IsEmpty())
      SetRoundedCornerAndInsets(corner_radius_, insets_);
    return old_layer;
  }

  // Update the rounded corner and insets with animation. |show| is true when
  // the target rounded corner radius and insets are for showing the indicator
  // circle.
  void AnimateRoundedCornerAndInsets(const AppListConfig& config, bool show) {
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTweenType(gfx::Tween::EASE_IN);
    settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(125));

    SetRoundedCornerAndInsets(
        show ? config.folder_unclipped_icon_dimension() / 2
             : config.folder_icon_dimension() / 2,
        show ? gfx::Insets() : gfx::Insets(config.folder_icon_insets()));
  }

  // Sets the rounded corner and the clip insets.
  void SetRoundedCornerAndInsets(int corner_radius, const gfx::Insets& insets) {
    EnsureLayer();
    layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
    if (insets.IsEmpty()) {
      layer()->SetClipRect(GetLocalBounds());
    } else {
      gfx::Rect bounds = GetLocalBounds();
      bounds.Inset(insets);
      layer()->SetClipRect(bounds);
    }

    // Save the attributes in case the layer is recreated.
    corner_radius_ = corner_radius;
    insets_ = insets;
  }

  // Ensure that the view has a layer.
  void EnsureLayer() {
    if (!layer()) {
      SetPaintToLayer();
      layer()->SetFillsBoundsOpaquely(false);
    }
  }

 private:
  // The rounded corner radius.
  int corner_radius_ = 0;

  // The insets to be clipped.
  gfx::Insets insets_;

  DISALLOW_COPY_AND_ASSIGN(IconImageView);
};

// static
const char AppListItemView::kViewClassName[] = "ui/app_list/AppListItemView";

AppListItemView::AppListItemView(AppsGridView* apps_grid_view,
                                 AppListItem* item,
                                 AppListViewDelegate* delegate)
    : AppListItemView(apps_grid_view, item, delegate, item->IsInFolder()) {}

AppListItemView::AppListItemView(AppsGridView* apps_grid_view,
                                 AppListItem* item,
                                 AppListViewDelegate* delegate,
                                 bool is_in_folder)
    : Button(apps_grid_view),
      is_folder_(item->GetItemType() == AppListFolderItem::kItemType),
      item_weak_(item),
      delegate_(delegate),
      apps_grid_view_(apps_grid_view),
      icon_(new IconImageView),
      title_(new views::Label),
      progress_bar_(new views::ProgressBar) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  if (is_folder_) {
    // Set background blur for folder icon and use mask layer to clip it into
    // circle. Note that blur is only enabled in tablet mode to improve dragging
    // smoothness.
    if (apps_grid_view_->IsTabletMode())
      SetBackgroundBlurEnabled(true);
    icon_->SetRoundedCornerAndInsets(
        GetAppListConfig().folder_icon_radius(),
        gfx::Insets(GetAppListConfig().folder_icon_insets()));
  }

  if (!is_in_folder && !is_folder_) {
    // To display shadow for icon while not affecting the icon's bounds, icon
    // shadow is behind the icon.
    icon_shadow_ = new views::ImageView;
    icon_shadow_->set_can_process_events_within_subtree(false);
    icon_shadow_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
    AddChildView(icon_shadow_);
  }

  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  title_->SetHandlesTooltips(false);
  title_->SetFontList(GetAppListConfig().app_title_font());
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetEnabledColor(apps_grid_view_->is_in_folder()
                              ? kFolderGridTitleColor
                              : GetAppListConfig().grid_title_color());
  if (!is_in_folder) {
    gfx::ShadowValues title_shadow = gfx::ShadowValues(
        1,
        gfx::ShadowValue(gfx::Vector2d(), kTitleShadowBlur, kTitleShadowColor));
    title_->SetShadows(title_shadow);
    title_shadow_margins_ = gfx::ShadowValue::GetMargin(title_shadow);
  }

  AddChildView(icon_);
  AddChildView(title_);
  AddChildView(progress_bar_);

  SetIcon(item->GetIcon(GetAppListConfig().type()));
  SetItemName(base::UTF8ToUTF16(item->GetDisplayName()),
              base::UTF8ToUTF16(item->name()));
  SetItemIsInstalling(item->is_installing());
  item->AddObserver(this);

  set_context_menu_controller(this);

  SetAnimationDuration(base::TimeDelta());

  preview_circle_radius_ = 0;
}

AppListItemView::~AppListItemView() {
  if (item_weak_)
    item_weak_->RemoveObserver(this);
}

void AppListItemView::SetIcon(const gfx::ImageSkia& icon) {
  // Clear icon and bail out if item icon is empty.
  if (icon.isNull()) {
    icon_->SetImage(nullptr);
    if (icon_shadow_)
      icon_shadow_->SetImage(nullptr);
    return;
  }

  gfx::ImageSkia resized = gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      is_folder_ ? GetAppListConfig().folder_unclipped_icon_size()
                 : GetAppListConfig().grid_icon_size());
  icon_->SetImage(resized);

  if (icon_shadow_) {
    // Create a shadow for the shown icon.
    gfx::ImageSkia shadowed =
        gfx::ImageSkiaOperations::CreateImageWithDropShadow(
            resized, gfx::ShadowValues(
                         1, gfx::ShadowValue(gfx::Vector2d(), kIconShadowBlur,
                                             kIconShadowColor)));
    icon_shadow_->SetImage(shadowed);
  }

  Layout();
}

void AppListItemView::SetUIState(UIState ui_state) {
  if (ui_state_ == ui_state)
    return;

  ui_state_ = ui_state;

  switch (ui_state_) {
    case UI_STATE_NORMAL:
      title_->SetVisible(!is_installing_);
      progress_bar_->SetVisible(is_installing_);
      ScaleAppIcon(false);
      break;
    case UI_STATE_DRAGGING:
      title_->SetVisible(false);
      progress_bar_->SetVisible(false);
      ScaleAppIcon(true);
      break;
    case UI_STATE_DROPPING_IN_FOLDER:
      break;
  }

  SchedulePaint();
}

void AppListItemView::ScaleAppIcon(bool scale_up) {
  if (!layer())
    return;
  const gfx::Rect bounds(layer()->bounds().size());
  gfx::Transform transform =
      gfx::GetScaleTransform(bounds.CenterPoint(), kDragDropAppIconScale);

  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds((kDragDropAppIconScaleTransitionInMs)));
  if (scale_up)
    layer()->SetTransform(transform);
  else
    layer()->SetTransform(gfx::Transform());
}

void AppListItemView::SetTouchDragging(bool touch_dragging) {
  if (touch_dragging_ == touch_dragging)
    return;

  touch_dragging_ = touch_dragging;

  SetState(STATE_NORMAL);
  SetUIState(touch_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);

  // EndDrag may delete |this|.
  if (!touch_dragging)
    apps_grid_view_->EndDrag(false);
}

void AppListItemView::SetMouseDragging(bool mouse_dragging) {
  mouse_dragging_ = mouse_dragging;

  SetState(STATE_NORMAL);
  SetUIState(mouse_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);

  if (!mouse_dragging_) {
    mouse_drag_proxy_created_ = false;

    // EndDrag may delete |this|.
    apps_grid_view_->EndDrag(false);
  }
}

void AppListItemView::OnMouseDragTimer() {
  // Show scaled up app icon to indicate draggable state.
  SetMouseDragging(true);
}

void AppListItemView::OnTouchDragTimer(
    const gfx::Point& tap_down_location,
    const gfx::Point& tap_down_root_location) {
  // Show scaled up app icon to indicate draggable state.
  apps_grid_view_->InitiateDrag(this, AppsGridView::TOUCH, tap_down_location,
                                tap_down_root_location);
  SetTouchDragging(true);
}

void AppListItemView::CancelContextMenu() {
  if (!context_menu_)
    return;

  menu_close_initiated_from_drag_ = true;
  context_menu_->Cancel();
}

void AppListItemView::OnDragEnded() {
  mouse_drag_timer_.Stop();
  touch_drag_timer_.Stop();
  SetUIState(UI_STATE_NORMAL);
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

const AppListConfig& AppListItemView::GetAppListConfig() const {
  return apps_grid_view_->GetAppListConfig();
}

void AppListItemView::SetItemName(const base::string16& display_name,
                                  const base::string16& full_name) {
  const base::string16 folder_name_placeholder =
      ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
          IDS_APP_LIST_FOLDER_NAME_PLACEHOLDER);
  if (is_folder_ && display_name.empty())
    title_->SetText(folder_name_placeholder);
  else
    title_->SetText(display_name);

  tooltip_text_ = display_name == full_name ? base::string16() : full_name;

  // Use full name for accessibility.
  SetAccessibleName(
      is_folder_ ? l10n_util::GetStringFUTF16(
                       IDS_APP_LIST_FOLDER_BUTTON_ACCESSIBILE_NAME,
                       full_name.empty() ? folder_name_placeholder : full_name)
                 : full_name);
  Layout();
}

void AppListItemView::SetItemIsInstalling(bool is_installing) {
  is_installing_ = is_installing;
  if (ui_state_ == UI_STATE_NORMAL) {
    title_->SetVisible(!is_installing);
    progress_bar_->SetVisible(is_installing);
  }
  SchedulePaint();
}

void AppListItemView::SetItemPercentDownloaded(int percent_downloaded) {
  // A percent_downloaded() of -1 can mean it's not known how much percent is
  // completed, or the download hasn't been marked complete, as is the case
  // while an extension is being installed after being downloaded.
  if (percent_downloaded == -1)
    return;
  progress_bar_->SetValue(percent_downloaded / 100.0);
}

void AppListItemView::OnContextMenuModelReceived(
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  waiting_for_context_menu_options_ = false;
  if (!menu_model || (context_menu_ && context_menu_->IsShowingMenu()))
    return;

  // GetContextMenuModel is asynchronous and takes a nontrivial amount of time
  // to complete. If a menu is shown after the icon has moved, |apps_grid_view_|
  // gets put in a bad state because the context menu begins to receive drag
  // events, interrupting the app icon drag.
  if (apps_grid_view_->IsDragViewMoved(*this))
    return;

  menu_show_initiated_from_key_ = source_type == ui::MENU_SOURCE_KEYBOARD;

  if (!apps_grid_view_->IsSelectedView(this))
    apps_grid_view_->ClearAnySelectedView();

  int run_types = views::MenuRunner::HAS_MNEMONICS |
                  views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                  views::MenuRunner::FIXED_ANCHOR |
                  views::MenuRunner::CONTEXT_MENU;

  if (source_type == ui::MENU_SOURCE_TOUCH && touch_dragging_)
    run_types |= views::MenuRunner::SEND_GESTURE_EVENTS_TO_OWNER;

  gfx::Rect anchor_rect =
      parent()->GetMirroredRect(apps_grid_view_->GetIdealBounds(this));
  // Anchor the menu to the same rect that is used for selection highlight.
  AdaptBoundsForSelectionHighlight(&anchor_rect);
  views::View::ConvertRectToScreen(parent(), &anchor_rect);

  AppLaunchedMetricParams metric_params = {
      ash::AppListLaunchedFrom::kLaunchedFromGrid};
  delegate_->GetAppLaunchedMetricParams(&metric_params);

  context_menu_ = std::make_unique<AppListMenuModelAdapter>(
      item_weak_->GetMetadata()->id, std::move(menu_model), GetWidget(),
      source_type, metric_params, AppListMenuModelAdapter::FULLSCREEN_APP_GRID,
      base::BindOnce(&AppListItemView::OnMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      apps_grid_view_->IsTabletMode());
  context_menu_->Run(anchor_rect, views::MenuAnchorPosition::kBubbleRight,
                     run_types);
  apps_grid_view_->SetSelectedView(this);
}

void AppListItemView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (context_menu_ && context_menu_->IsShowingMenu())
    return;
  // Prevent multiple requests for context menus before the current request
  // completes. If a second request is sent before the first one can respond,
  // the Chrome side delegate will become unresponsive
  // (https://crbug.com/881886).
  if (waiting_for_context_menu_options_)
    return;
  waiting_for_context_menu_options_ = true;
  delegate_->GetContextMenuModel(
      item_weak_->id(),
      base::BindOnce(&AppListItemView::OnContextMenuModelReceived,
                     weak_ptr_factory_.GetWeakPtr(), point, source_type));
}

bool AppListItemView::ShouldEnterPushedState(const ui::Event& event) {
  // Don't enter pushed state for ET_GESTURE_TAP_DOWN so that hover gray
  // background does not show up during scroll.
  if (event.type() == ui::ET_GESTURE_TAP_DOWN)
    return false;

  return views::Button::ShouldEnterPushedState(event);
}

void AppListItemView::PaintButtonContents(gfx::Canvas* canvas) {
  if (apps_grid_view_->IsDraggedView(this))
    return;

  // TODO(ginko) focus and selection should be unified.
  if ((apps_grid_view_->IsSelectedView(this) || HasFocus()) &&
      (delegate_->KeyboardTraversalEngaged() ||
       (context_menu_ && context_menu_->IsShowingMenu()))) {
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    if (delegate_->KeyboardTraversalEngaged()) {
      flags.setColor(apps_grid_view_->is_in_folder()
                         ? kFolderGridFocusRingColor
                         : GetAppListConfig().grid_selected_color());
      flags.setStyle(cc::PaintFlags::kStroke_Style);
      flags.setStrokeWidth(kFocusRingWidth);
    } else {
      // If a context menu is open, we should instead use a grey selection.
      flags.setColor(apps_grid_view_->is_in_folder() ? kContextSelectionFolder
                                                     : kContextSelection);
      flags.setStyle(cc::PaintFlags::kFill_Style);
    }
    gfx::Rect selection_highlight_bounds = GetContentsBounds();
    AdaptBoundsForSelectionHighlight(&selection_highlight_bounds);
    canvas->DrawRoundRect(gfx::RectF(selection_highlight_bounds),
                          GetAppListConfig().grid_focus_corner_radius(), flags);
  }

  const int preview_circle_radius = GetPreviewCircleRadius();
  if (!preview_circle_radius)
    return;

  // Draw folder dropping preview circle.
  gfx::Point center = gfx::Point(icon_->x() + icon_->size().width() / 2,
                                 icon_->y() + icon_->size().height() / 2);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setAntiAlias(true);
  flags.setColor(GetAppListConfig().folder_bubble_color());
  canvas->DrawCircle(center, preview_circle_radius, flags);
}

bool AppListItemView::OnMousePressed(const ui::MouseEvent& event) {
  Button::OnMousePressed(event);

  if (!ShouldEnterPushedState(event))
    return true;

  apps_grid_view_->InitiateDrag(this, AppsGridView::MOUSE, event.location(),
                                event.root_location());

  if (apps_grid_view_->IsDraggedView(this)) {
    mouse_drag_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kMouseDragUIDelayInMs),
        this, &AppListItemView::OnMouseDragTimer);
  }
  return true;
}

const char* AppListItemView::GetClassName() const {
  return kViewClassName;
}

void AppListItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  const gfx::Rect icon_bounds = GetIconBoundsForTargetViewBounds(
      GetAppListConfig(), rect, icon_->GetImage().size());
  icon_->SetBoundsRect(icon_bounds);

  if (icon_shadow_) {
    const gfx::Rect icon_shadow_bounds = GetIconBoundsForTargetViewBounds(
        GetAppListConfig(), rect, icon_shadow_->size());
    icon_shadow_->SetBoundsRect(icon_shadow_bounds);
  }

  gfx::Rect title_bounds = GetTitleBoundsForTargetViewBounds(
      GetAppListConfig(), rect, title_->GetPreferredSize());
  if (!apps_grid_view_->is_in_folder())
    title_bounds.Inset(title_shadow_margins_);
  title_->SetBoundsRect(title_bounds);

  progress_bar_->SetBoundsRect(GetProgressBarBoundsForTargetViewBounds(
      rect, progress_bar_->GetPreferredSize()));
}

gfx::Size AppListItemView::CalculatePreferredSize() const {
  return gfx::Size(GetAppListConfig().grid_tile_width(),
                   GetAppListConfig().grid_tile_height());
}

bool AppListItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Disable space key to press the button. The keyboard events received
  // by this view are forwarded from a Textfield (SearchBoxView) and key
  // released events are not forwarded. This leaves the button in pressed
  // state.
  if (event.key_code() == ui::VKEY_SPACE)
    return false;

  return Button::OnKeyPressed(event);
}

void AppListItemView::OnMouseReleased(const ui::MouseEvent& event) {
  Button::OnMouseReleased(event);
  SetMouseDragging(false);
}

bool AppListItemView::OnMouseDragged(const ui::MouseEvent& event) {
  Button::OnMouseDragged(event);
  if (apps_grid_view_->IsDraggedView(this) && mouse_dragging_) {
    // Update the drag location of the drag proxy if it has been created.
    // If the drag is no longer happening, it could be because this item
    // got removed, in which case this item has been destroyed. So, bail out
    // now as there will be nothing else to do anyway as
    // apps_grid_view_->dragging() will be false.
    if (!apps_grid_view_->UpdateDragFromItem(AppsGridView::MOUSE, event))
      return true;
  }

  if (!apps_grid_view_->IsSelectedView(this))
    apps_grid_view_->ClearAnySelectedView();

  // Show dragging UI when it's confirmed without waiting for the timer.
  if (ui_state_ != UI_STATE_DRAGGING && apps_grid_view_->dragging() &&
      apps_grid_view_->IsDraggedView(this)) {
    mouse_drag_timer_.Stop();
    SetUIState(UI_STATE_DRAGGING);
  }
  return true;
}

bool AppListItemView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // Ensure accelerators take priority in the app list. This ensures, e.g., that
  // Ctrl+Space will switch input methods rather than activate the button.
  return false;
}

void AppListItemView::OnFocus() {
  if (focus_silently_)
    return;
  apps_grid_view_->SetSelectedView(this);
}

void AppListItemView::OnBlur() {
  SchedulePaint();
  apps_grid_view_->ClearSelectedView(this);
}

void AppListItemView::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (touch_dragging_) {
        CancelContextMenu();
        apps_grid_view_->StartDragAndDropHostDragAfterLongPress(
            AppsGridView::TOUCH);
        event->SetHandled();
      } else {
        touch_drag_timer_.Stop();
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (touch_dragging_ && apps_grid_view_->IsDraggedView(this)) {
        apps_grid_view_->UpdateDragFromItem(AppsGridView::TOUCH, *event);
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
      if (state() != STATE_DISABLED) {
        SetState(STATE_PRESSED);
        touch_drag_timer_.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(kTouchLongpressDelayInMs),
            base::Bind(&AppListItemView::OnTouchDragTimer,
                       base::Unretained(this), event->location(),
                       event->root_location()));
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_TAP:
    case ui::ET_GESTURE_TAP_CANCEL:
      if (state() != STATE_DISABLED) {
        touch_drag_timer_.Stop();
        SetState(STATE_NORMAL);
      }
      break;
    case ui::ET_GESTURE_LONG_TAP:
    case ui::ET_GESTURE_END:
      touch_drag_timer_.Stop();
      SetTouchDragging(false);
      if (context_menu_ && context_menu_->IsShowingMenu())
        apps_grid_view_->SetSelectedView(this);
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
  if (!event->handled())
    Button::OnGestureEvent(event);
}

base::string16 AppListItemView::GetTooltipText(const gfx::Point& p) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
  title_->SetTooltipText(tooltip_text_);
  base::string16 tooltip = title_->GetTooltipText(p);
  title_->SetHandlesTooltips(false);
  return tooltip;
}

void AppListItemView::OnDraggedViewEnter() {
  if (is_folder_) {
    icon_->AnimateRoundedCornerAndInsets(GetAppListConfig(), true);
    return;
  }
  CreateDraggedViewHoverAnimation();
  dragged_view_hover_animation_->Show();
}

void AppListItemView::OnDraggedViewExit() {
  if (is_folder_) {
    icon_->AnimateRoundedCornerAndInsets(GetAppListConfig(), false);
    return;
  }
  CreateDraggedViewHoverAnimation();
  dragged_view_hover_animation_->Hide();
}

void AppListItemView::SetBackgroundBlurEnabled(bool enabled) {
  DCHECK(is_folder_);
  if (enabled)
    icon_->EnsureLayer();
  icon_->layer()->SetBackgroundBlur(enabled ? GetAppListConfig().blur_radius()
                                            : 0);
}

void AppListItemView::EnsureLayer() {
  if (layer())
    return;
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void AppListItemView::FireMouseDragTimerForTest() {
  mouse_drag_timer_.FireNow();
}

void AppListItemView::AnimationProgressed(const gfx::Animation* animation) {
  DCHECK(!is_folder_);

  preview_circle_radius_ = gfx::Tween::IntValueBetween(
      animation->GetCurrentValue(), 0,
      GetAppListConfig().folder_dropping_circle_radius());
  SchedulePaint();
}

void AppListItemView::OnMenuClosed() {
  // Release menu since its menu model delegate (AppContextMenu) could be
  // released as a result of menu command execution.
  context_menu_.reset();

  if (!menu_close_initiated_from_drag_) {
    // If the menu was not closed due to a drag sequence(e.g. multi touch) reset
    // the drag state.
    SetState(STATE_NORMAL);
    SetTouchDragging(false);
  }

  menu_close_initiated_from_drag_ = false;

  // Keep the item focused if the menu was shown via keyboard.
  if (!menu_show_initiated_from_key_)
    OnBlur();
}

void AppListItemView::OnSyncDragEnd() {
  SetUIState(UI_STATE_NORMAL);
}

gfx::Rect AppListItemView::GetIconBounds() const {
  if (is_folder_) {
    // The folder icon is in unclipped size, so clip it before return.
    gfx::Rect folder_icon_bounds = icon_->bounds();
    folder_icon_bounds.ClampToCenteredSize(
        GetAppListConfig().folder_icon_size());
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
  if (!is_folder_)
    return icon_->GetImage();

  return gfx::CanvasImageSource::MakeImageSkia<ClippedFolderIconImageSource>(
      GetAppListConfig().folder_icon_size(), icon_->GetImage());
}

void AppListItemView::SetIconVisible(bool visible) {
  icon_->SetVisible(visible);
  if (icon_shadow_)
    icon_shadow_->SetVisible(visible);
}

void AppListItemView::SetDragUIState() {
  SetUIState(UI_STATE_DRAGGING);
}

// static
gfx::Rect AppListItemView::GetIconBoundsForTargetViewBounds(
    const AppListConfig& config,
    const gfx::Rect& target_bounds,
    const gfx::Size& icon_size) {
  gfx::Rect rect(target_bounds);
  rect.Inset(0, 0, 0, config.grid_icon_bottom_padding());
  rect.ClampToCenteredSize(icon_size);
  return rect;
}

// static
gfx::Rect AppListItemView::GetTitleBoundsForTargetViewBounds(
    const AppListConfig& config,
    const gfx::Rect& target_bounds,
    const gfx::Size& title_size) {
  gfx::Rect rect(target_bounds);
  rect.Inset(config.grid_title_horizontal_padding(),
             config.grid_title_top_padding(),
             config.grid_title_horizontal_padding(),
             config.grid_title_bottom_padding());
  rect.ClampToCenteredSize(title_size);
  // Respect the title preferred height, to ensure the text does not get clipped
  // due to padding if the item view gets too small.
  if (rect.height() < title_size.height()) {
    rect.set_y(rect.y() - (title_size.height() - rect.height()) / 2);
    rect.set_height(title_size.height());
  }
  return rect;
}

// static
gfx::Rect AppListItemView::GetProgressBarBoundsForTargetViewBounds(
    const gfx::Rect& target_bounds,
    const gfx::Size& progress_bar_size) {
  gfx::Rect progress_bar_bounds(progress_bar_size);
  progress_bar_bounds.set_x(
      (target_bounds.width() - progress_bar_bounds.width()) / 2);
  progress_bar_bounds.set_y(target_bounds.y());
  return progress_bar_bounds;
}

void AppListItemView::ItemIconChanged(ash::AppListConfigType config_type) {
  if (config_type != ash::AppListConfigType::kShared &&
      config_type != GetAppListConfig().type()) {
    return;
  }
  DCHECK(item_weak_);
  SetIcon(item_weak_->GetIcon(GetAppListConfig().type()));
}

void AppListItemView::ItemNameChanged() {
  SetItemName(base::UTF8ToUTF16(item_weak_->GetDisplayName()),
              base::UTF8ToUTF16(item_weak_->name()));
}

void AppListItemView::ItemIsInstallingChanged() {
  SetItemIsInstalling(item_weak_->is_installing());
}

void AppListItemView::ItemPercentDownloadedChanged() {
  SetItemPercentDownloaded(item_weak_->percent_downloaded());
}

void AppListItemView::ItemBeingDestroyed() {
  DCHECK(item_weak_);
  item_weak_->RemoveObserver(this);
  item_weak_ = nullptr;
}

int AppListItemView::GetPreviewCircleRadius() const {
  return is_folder_ ? 0 : preview_circle_radius_;
}

void AppListItemView::CreateDraggedViewHoverAnimation() {
  DCHECK(!is_folder_);

  if (dragged_view_hover_animation_)
    return;

  dragged_view_hover_animation_ = std::make_unique<gfx::SlideAnimation>(this);
  dragged_view_hover_animation_->SetTweenType(gfx::Tween::EASE_IN);
  dragged_view_hover_animation_->SetSlideDuration(
      base::TimeDelta::FromMilliseconds(250));
}

void AppListItemView::AdaptBoundsForSelectionHighlight(gfx::Rect* bounds) {
  bounds->Inset(0, 0, 0, GetAppListConfig().grid_icon_bottom_padding());
  bounds->ClampToCenteredSize(GetAppListConfig().grid_focus_size());
  // Update the bounds to account for the focus ring width - by default, the
  // focus ring is painted so the highlight bounds are centered within the
  // focus ring stroke - this should be overridden so the outer stroke bounds
  // match the grid focus size set in the app list config.
  bounds->Inset(gfx::Insets(kFocusRingWidth / 2));
}

}  // namespace ash
