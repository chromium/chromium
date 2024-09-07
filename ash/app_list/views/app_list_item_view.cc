// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_item_view.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/app_list/app_collections_constants.h"
#include "ash/app_list/app_list_item_util.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/apps_collections_controller.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/folder_image.h"
#include "ash/app_list/views/app_list_menu_model_adapter.h"
#include "ash/app_list/views/apps_grid_context_menu.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/dot_indicator.h"
#include "ash/style/style_util.h"
#include "ash/style/system_textfield.h"
#include "ash/style/typography.h"
#include "ash/system/progress_indicator/progress_indicator.h"
#include "ash/user_education/user_education_class_properties.h"
#include "ash/user_education/user_education_controller.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "cc/paint/paint_flags.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/utils/haptics_util.h"
#include "components/services/app_service/public/cpp/app_shortcut_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/insets_f.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Delay in milliseconds of when the dragging UI should be shown for mouse drag.
constexpr int kMouseDragUIDelayInMs = 200;

// Delay in milliseconds of when the dragging UI should be shown for touch drag.
// Note: For better user experience, this is made shorter than
// EventType::kGestureLongPress delay, which is too long for this case, e.g.,
// about 650ms.
constexpr int kTouchLongpressDelayInMs = 300;

// For touch initiated dragging, shift the cursor anchor point of the scaled
// icon by the following:
static const int kTouchDragImageVerticalOffset = 25;

// The drag and drop app icon should get scaled by this factor.
constexpr float kDragDropAppIconScale = 1.2f;

// The promise app placeholder icon should use this size.
constexpr int kPlaceholderIconDimension = 24;

// The width of the promise app progress ring.
constexpr int kPromiseRingStrokeSize = 2;

// The duration of the animation to animate an app list item view in as a
// promise app replacement.
constexpr base::TimeDelta kSwapPromiseIconDuration = base::Milliseconds(100);

// The amount of space between the progress ring and the promise app background
// and icon depending on the app_state.
constexpr gfx::Insets kProgressRingMarginInstalling = gfx::Insets(-2);
constexpr gfx::Insets kProgressRingMarginPending = gfx::Insets(-3);

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

// The maximum number that can be shown on the item counter in refreshed folder
// icons.
constexpr size_t kMaxItemCounterCount = 100u;

// Creates a badged app shortcut image for the provided `app_list_config` from
// the shortcut's `main_icon` and the `badge_icon`.
gfx::ImageSkia CreateBadgedShortcutImage(
    const AppListConfig& app_list_config,
    const gfx::ImageSkia& main_icon,
    const gfx::ImageSkia& badge_icon,
    float icon_scale,
    const ui::ColorProvider* color_provider) {
  const gfx::Size badge_icon_size =
      gfx::Size(app_list_config.shortcut_host_badge_icon_dimension(),
                app_list_config.shortcut_host_badge_icon_dimension());
  const int background_diameter =
      app_list_config.GetShortcutBackgroundContainerDimension();
  gfx::ImageSkia icon_with_badge =
      apps::AppShortcutImage::CreateImageWithBadgeAndTeardropBackground(
          background_diameter / 2,
          app_list_config.GetShortcutTeardropCornerRadius(),
          app_list_config.GetShortcutHostBadgeIconContainerDimension() / 2,
          color_provider->GetColor(cros_tokens::kCrosSysSystemOnBaseOpaque),
          gfx::ImageSkiaOperations::CreateResizedImage(
              main_icon, skia::ImageOperations::RESIZE_BEST,
              app_list_config.GetShortcutIconSize()),
          gfx::ImageSkiaOperations::CreateResizedImage(
              badge_icon, skia::ImageOperations::RESIZE_BEST, badge_icon_size));
  return gfx::ImageSkiaOperations::CreateResizedImage(
      icon_with_badge, skia::ImageOperations::RESIZE_BEST,
      gfx::ScaleToRoundedSize(
          gfx::Size(background_diameter, background_diameter), icon_scale));
}

// Draws a circular background for a promise icon view.
class PromiseIconBackground : public views::Background {
 public:
  PromiseIconBackground(ui::ColorId color_id,
                        const gfx::Rect& icon_bounds,
                        const gfx::Insets& insets)
      : color_id_(color_id), icon_bounds_(icon_bounds), insets_(insets) {}

  PromiseIconBackground(const PromiseIconBackground&) = delete;
  PromiseIconBackground& operator=(const PromiseIconBackground&) = delete;
  ~PromiseIconBackground() override = default;

  // views::Background:
  void Paint(gfx::Canvas* canvas, views::View* view) const override {
    gfx::RectF bounds = gfx::RectF(icon_bounds_);
    bounds.Inset(gfx::InsetsF(insets_));

    const float radius =
        std::min(bounds.size().width(), bounds.size().height()) / 2.f;

    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(get_color());

    canvas->DrawCircle(bounds.CenterPoint(), radius, flags);
  }

  void OnViewThemeChanged(views::View* view) override {
    SetNativeControlColor(view->GetColorProvider()->GetColor(color_id_));
    view->SchedulePaint();
  }

 private:
  const ui::ColorId color_id_;
  const gfx::Rect icon_bounds_;
  const gfx::Insets insets_;
};

// Draws a dot with no shadow.
class DotView : public views::View {
  METADATA_HEADER(DotView, views::View)

 public:
  DotView() : color_id_(cros_tokens::kCrosSysTertiary) {
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
    flags.setColor(GetColorProvider()->GetColor(color_id_));
    flags.setAntiAlias(true);
    canvas->DrawCircle(center, scale * radius, flags);
  }

  void OnThemeChanged() override {
    views::View::OnThemeChanged();
    SchedulePaint();
  }

 private:
  const ui::ColorId color_id_;
};

BEGIN_METADATA(DotView)
END_METADATA

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

bool IsReorderCommand(int command_id) {
  CommandId command = static_cast<CommandId>(command_id);

  return (command == CommandId::REORDER_BY_NAME_ALPHABETICAL ||
          command == CommandId::REORDER_BY_NAME_REVERSE_ALPHABETICAL ||
          command == CommandId::REORDER_BY_COLOR);
}

}  // namespace

class AppListItemView::FolderIconView : public views::View,
                                        public AppListItemListObserver {
  METADATA_HEADER(FolderIconView, views::View)

 public:
  FolderIconView(AppListFolderItem* folder_item,
                 const AppListConfig* config,
                 float icon_scale)
      : folder_item_(folder_item),
        config_(config),
        icon_scale_(icon_scale) {
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    folder_item_->item_list()->AddObserver(this);
  }
  FolderIconView(const FolderIconView&) = delete;
  FolderIconView& operator=(const FolderIconView&) = delete;

  ~FolderIconView() override {
    if (folder_item_) {
      folder_item_->item_list()->RemoveObserver(this);
    }
  }

  void ResetFolderItem() { folder_item_ = nullptr; }
  void UpdateAppListConfig(const AppListConfig* config) { config_ = config; }
  void SetIconScale(float scale) {
    icon_scale_ = scale;
    SchedulePaint();
  }

  AppListItem* GetDraggedItem() const {
    return folder_item_ ? folder_item_->item_list()->FindItem(dragged_item_id_)
                        : nullptr;
  }

  // Sets the current dragged item to the item with id `item_id`.
  // `dragged_item_` could be null if such item doesn't exist in `folder_item_`.
  void UpdateDraggedItem(const std::string& item_id) {
    dragged_item_id_ = item_id;
    SchedulePaint();
  }

  // The count shows on the item counter is the number of items that aren't
  // drawn on the folder icon. Returns nullopt if the counter should not be
  // drawn.
  std::optional<size_t> GetItemCounterCount() const {
    size_t item_count = folder_item_->item_list()->item_count();
    size_t icons_in_folder = GetDraggedItem() ? item_count - 1 : item_count;
    if (icons_in_folder <= FolderImage::kNumFolderTopItems) {
      return std::nullopt;
    }

    size_t count = icons_in_folder - (FolderImage::kNumFolderTopItems - 1);
    return std::min(count, kMaxItemCounterCount);
  }

  gfx::ImageSkia CreateDragImage() {
    const views::Widget* widget = GetWidget();
    const float scale = widget->GetCompositor()->device_scale_factor();
    const gfx::Rect paint_bounds(gfx::ScaleToCeiledSize(
        config_->folder_icon_size(), kDragDropAppIconScale * scale));
    const bool is_pixel_canvas = widget->GetCompositor()->is_pixel_canvas();
    SkBitmap bitmap;
    bitmap.allocN32Pixels(paint_bounds.width(), paint_bounds.height());
    bitmap.eraseColor(SK_ColorTRANSPARENT);

    // Draw the background circle of the icon.
    SkCanvas canvas(bitmap);
    SkPaint background_circle;
    const ui::ColorId color_id = cros_tokens::kCrosSysSystemOnBase;
    background_circle.setColor(GetColorProvider()->GetColor(color_id));
    background_circle.setStyle(SkPaint::kFill_Style);
    background_circle.setAntiAlias(true);

    gfx::Point center = paint_bounds.CenterPoint();
    canvas.drawCircle(
        center.x(), center.y(),
        config_->icon_visible_dimension() * kDragDropAppIconScale * scale / 2,
        background_circle);

    auto list = base::MakeRefCounted<cc::DisplayItemList>();
    ui::PaintContext context(list.get(), scale, paint_bounds, is_pixel_canvas);
    {
      base::AutoReset<float> forced_icon_scale(&icon_scale_,
                                               kDragDropAppIconScale);
      Paint(
          views::PaintInfo::CreateRootPaintInfo(context, paint_bounds.size()));
    }
    list->Finalize();
    list->Raster(&canvas, nullptr);

    return gfx::ImageSkia::CreateFromBitmap(bitmap, scale);
  }

 private:
  // The scale factor that resize the item counter to match the visual size of
  // other app icons.
  static constexpr float kItemCounterSizeFactor = 0.93f;

  // AppListItemListObserver:
  void OnListItemAdded(size_t index, AppListItem* item) override {
    SchedulePaint();
  }
  void OnListItemRemoved(size_t index, AppListItem* item) override {
    SchedulePaint();
  }
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override {
    // Only repaint if the move may reflect on the icon.
    size_t indices_affected = GetDraggedItem()
                                  ? FolderImage::kNumFolderTopItems + 1
                                  : FolderImage::kNumFolderTopItems;
    if (from_index < indices_affected || to_index < indices_affected) {
      SchedulePaint();
    }
  }

  void DrawItemCounter(gfx::Canvas* canvas,
                       const gfx::Rect& bounds,
                       int count) {
    const float item_icon_size = config_->item_icon_in_folder_icon_dimension() *
                                 kItemCounterSizeFactor * icon_scale_;
    const float counter_radius = item_icon_size / 2.f;

    // Draw the item counter background circle.
    const gfx::PointF draw_center(bounds.CenterPoint());

    cc::PaintFlags flags;
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setAntiAlias(true);
    flags.setColor(GetColorProvider()->GetColor(cros_tokens::kCrosSysPrimary));
    canvas->DrawCircle(draw_center, counter_radius, flags);

    // Paint the number of apps that are not showing in the folder icon.
    const std::u16string text = base::NumberToString16(count);
    gfx::FontList font_list = config_->item_counter_in_folder_icon_font();
    canvas->DrawStringRectWithFlags(
        text, font_list,
        GetColorProvider()->GetColor(cros_tokens::kCrosSysOnPrimary), bounds,
        gfx::Canvas::TEXT_ALIGN_CENTER);
  }

  void OnPaint(gfx::Canvas* canvas) override {
    const AppListItemList* item_list = folder_item_->item_list();
    size_t num_items = item_list->item_count();

    // Exclude the dragged item that is dragged from the folder.
    if (GetDraggedItem()) {
      --num_items;
    }

    if (num_items == 0) {
      return;
    }

    // Draw top items' icons.
    const size_t num_icons =
        std::min(FolderImage::kNumFolderTopItems, num_items);
    std::vector<gfx::Rect> top_icon_bounds = GetTopIconsBounds(num_icons);
    auto item_count = GetItemCounterCount();

    // `icon_pos` is the position index on the folder icon for the app icons and
    // item counter to be drawn, while `item_idx_drawn` is the item index in the
    // folder that represent the icon to be drawn.
    for (size_t icon_pos = 0, item_idx_drawn = 0; icon_pos < num_icons;
         ++icon_pos, ++item_idx_drawn) {
      // Draw the item counter at the last position on the icon if needed.
      if (item_count.has_value() && icon_pos == num_icons - 1) {
        DrawItemCounter(canvas, top_icon_bounds[icon_pos], item_count.value());
        break;
      }

      // Do not draw the dragged app item on the folder icon.
      if (item_list->item_at(item_idx_drawn) == GetDraggedItem()) {
        // Retain the icon_pos for the next iteration and check the next item in
        // item list to see if it needs to be drawn.
        --icon_pos;
        continue;
      }

      const gfx::ImageSkia item_icon =
          item_list->item_at(item_idx_drawn)->GetIcon(config_->type());

      if (item_icon.isNull()) {
        continue;
      }

      const gfx::Rect bounds = top_icon_bounds[icon_pos];
      const gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
          item_icon, skia::ImageOperations::RESIZE_BEST, bounds.size()));
      canvas->DrawImageInt(resized, bounds.x(), bounds.y());
    }
  }

  std::vector<gfx::Rect> GetTopIconsBounds(size_t num_items) {
    gfx::Rect folder_icon_bounds(config_->folder_icon_size());
    std::vector<gfx::Rect> top_icon_bounds =
        FolderImage::GetTopIconsBounds(*config_, folder_icon_bounds, num_items);

    if (icon_scale_ == 1) {
      return top_icon_bounds;
    }
    std::for_each(top_icon_bounds.begin(), top_icon_bounds.end(),
                  [&](auto& bounds) {
                    bounds = gfx::ScaleToRoundedRect(bounds, icon_scale_);
                  });
    return top_icon_bounds;
  }

  // The folder item this icon view paints.
  raw_ptr<AppListFolderItem> folder_item_;

  raw_ptr<const AppListConfig, DanglingUntriaged> config_;

  // The scaling factor used for cardified states in tablet mode.
  float icon_scale_;

  // The id of the currently dragged app item in the folder.
  std::string dragged_item_id_;
};

// An AppMenuAdapter specific to AppListItems that are shown in the context of
// the AppsCollections. The adapter intercepts sort requests and delegates them
// to AppsCollectionsController.
class AppsCollectionsMenuModelAdapter : public AppListMenuModelAdapter {
 public:
  AppsCollectionsMenuModelAdapter(
      const std::string& app_id,
      std::unique_ptr<ui::SimpleMenuModel> menu_model,
      views::Widget* widget_owner,
      ui::MenuSourceType source_type,
      const AppLaunchedMetricParams& metric_params,
      AppListViewAppType type,
      base::OnceClosure on_menu_closed_callback,
      bool is_tablet_mode,
      AppCollection collection)
      : AppListMenuModelAdapter(app_id,
                                std::move(menu_model),
                                widget_owner,
                                source_type,
                                metric_params,
                                type,
                                std::move(on_menu_closed_callback),
                                is_tablet_mode,
                                collection) {}

  AppsCollectionsMenuModelAdapter(const AppsCollectionsMenuModelAdapter&) =
      delete;
  AppsCollectionsMenuModelAdapter& operator=(
      const AppsCollectionsMenuModelAdapter&) = delete;

  ~AppsCollectionsMenuModelAdapter() override = default;

  void ExecuteCommand(int id, int mouse_event_flags) override {
    // Intercept Reorder commands to show the reorder confirmation dialog.
    if (IsReorderCommand(id)) {
      AppsCollectionsController::Get()->RequestAppReorder(
          static_cast<CommandId>(id) == CommandId::REORDER_BY_COLOR
              ? AppListSortOrder::kColor
              : AppListSortOrder::kNameAlphabetical);
      return;
    }

    // Note that ExecuteCommand might delete us.
    AppListMenuModelAdapter::ExecuteCommand(id, mouse_event_flags);
  }
};

BEGIN_METADATA(AppListItemView, FolderIconView)
END_METADATA

AppListItemView::AppListItemView(const AppListConfig* app_list_config,
                                 AppListItemViewGridDelegate* grid_delegate,
                                 AppListItem* item,
                                 AppListViewDelegate* view_delegate,
                                 Context context)
    : views::Button(base::BindRepeating(
          &AppListItemViewGridDelegate::OnAppListItemViewActivated,
          base::Unretained(grid_delegate),
          base::Unretained(this))),
      app_list_config_(app_list_config),
      is_folder_(item->GetItemType() == AppListFolderItem::kItemType),
      item_weak_(item),
      grid_delegate_(grid_delegate),
      view_delegate_(view_delegate),
      use_item_icon_(!is_folder_),
      context_(context) {
  DCHECK(app_list_config_);
  DCHECK(grid_delegate_);
  DCHECK(view_delegate_);
  SetFocusBehavior(FocusBehavior::ALWAYS);
  set_suppress_default_focus_handling();
  GetViewAccessibility().SetIsLeaf(true);

  is_promise_app_ =
      item_weak_->GetMetadata()->app_status == AppStatus::kPending ||
      item_weak_->GetMetadata()->app_status == AppStatus::kInstalling;

  has_host_badge_ = !item_weak_->GetMetadata()->badge_icon.isNull();

  // Draw the promise ring for the first time before waiting for updates.
  if (is_promise_app_) {
    UpdateProgressIndicatorState();
  }

  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/false,
                                   /*highlight_on_focus=*/false,
                                   cros_tokens::kCrosSysRippleNeutralOnSubtle);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::OFF);

  SetHideInkDropWhenShowingContextMenu(false);
  SetShowInkDropWhenHotTracked(false);
  SetHasInkDropActionOnClick(false);

  views::FocusRing::Install(this);
  views::FocusRing* const focus_ring = views::FocusRing::Get(this);
  focus_ring->SetOutsetFocusRingDisabled(true);
  focus_ring->SetColorId(cros_tokens::kCrosSysFocusRing);
  focus_ring->SetHasFocusPredicate(base::BindRepeating([](const View* view) {
    const auto* v = views::AsViewClass<AppListItemView>(view);
    CHECK(v);

    // With a `view_delegate_` present, focus ring should only show when
    // button is focused and keyboard traversal is engaged.
    if (v->view_delegate_ && !v->view_delegate_->KeyboardTraversalEngaged()) {
      return false;
    }

    if (v->drag_state_ != DragState::kNone) {
      return false;
    }

    if (v->waiting_for_context_menu_options_ || v->IsShowingAppMenu()) {
      return false;
    }

    return v->HasFocus();
  }));

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(1), app_list_config_->grid_focus_corner_radius());

  auto title = std::make_unique<views::Label>();
  title->SetBackgroundColor(SK_ColorTRANSPARENT);
  title->SetHandlesTooltips(false);
  title->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  TypographyProvider::Get()->StyleLabel(
      app_list_config_->type() == AppListConfigType::kDense
          ? TypographyToken::kCrosAnnotation1
          : TypographyToken::kCrosButton2,
      *title);
  title->SetEnabledColorId(cros_tokens::kCrosSysOnSurface);

  icon_background_ = AddChildView(std::make_unique<views::View>());
  icon_background_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  icon_background_->layer()->SetFillsBoundsOpaquely(false);
  icon_background_->SetCanProcessEventsWithinSubtree(false);
  icon_background_->SetVisible(is_folder_);

  if (use_item_icon_) {
    // If the item icon is used, set the icon in ImageView and paint the view.
    icon_ = AddChildView(std::make_unique<views::ImageView>());
    icon_->SetCanProcessEventsWithinSubtree(false);
    icon_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);

    if (has_host_badge_ &&
        features::IsSeparateWebAppShortcutBadgeIconEnabled()) {
      SetIconAndMaybeHostBadgeIcon(icon_image_, item_weak_->GetHostBadgeIcon());
    }
  } else {
    // Refreshed folder icons are painted on FolderIconView directly instead of
    // using the folder item icon.
    folder_icon_ = AddChildView(std::make_unique<FolderIconView>(
        item_weak_->AsFolderItem(), app_list_config_, icon_scale_));
    folder_icon_->SetCanProcessEventsWithinSubtree(false);
  }

  if (is_folder_) {
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

  UpdateIconView(/*update_item_icon=*/true);
  SetItemName(base::UTF8ToUTF16(item->name()),
              base::UTF8ToUTF16(item->GetAccessibleName()));
  item->AddObserver(this);

  if (is_folder_) {
    context_menu_for_folder_ = std::make_unique<AppsGridContextMenu>(
        AppsGridContextMenu::GridType::kAppsGrid);
    set_context_menu_controller(context_menu_for_folder_.get());
  } else {
    set_context_menu_controller(this);
  }

  SetAnimationDuration(base::TimeDelta());

  preview_circle_radius_ = 0;

  if (UserEducationController::Get()) {
    switch (context) {
      case Context::kRecentAppsView:
        break;
      case Context::kAppsGridView:
      case Context::kAppsCollection:
        if (std::optional<ui::ElementIdentifier> element_identifier =
                UserEducationController::Get()->GetElementIdentifierForAppId(
                    item->id())) {
          // NOTE: Set `kHelpBubbleContextKey` before
          // `views::kElementIdentifierKey` in case registration causes a help
          // bubble to be created synchronously.
          SetProperty(kHelpBubbleContextKey, HelpBubbleContext::kAsh);
          SetProperty(views::kElementIdentifierKey, *element_identifier);
        }
        break;
    }
  }

  UpdateAccessibleDescription();
}

void AppListItemView::InitializeIconLoader() {
  DCHECK(item_weak_);
  // Creates app icon load helper. base::Unretained is safe because `this` owns
  // `icon_load_helper_` and `view_delegate_` outlives `this`.
  if (is_folder_) {
    AppListFolderItem* folder_item = item_weak_->AsFolderItem();
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
  set_context_menu_controller(nullptr);
  if (item_weak_) {
    item_weak_->RemoveObserver(this);
  }
  StopObservingImplicitAnimations();
}

void AppListItemView::UpdateIconView(bool update_item_icon) {
  if (!use_item_icon_) {
    folder_icon_->SetIconScale(icon_scale_);
    DeprecatedLayoutImmediately();
    return;
  }

  if (update_item_icon && item_weak_) {
    has_host_badge_ = !item_weak_->GetHostBadgeIcon().isNull();
  }

  if (update_item_icon) {
    if (ItemHasPlaceholderIcon()) {
      icon_image_model_ = ui::ImageModel(ui::ImageModel::FromVectorIcon(
          ash::kPlaceholderAppIcon, cros_tokens::kCrosSysPrimary));
    } else {
      icon_image_model_ = ui::ImageModel(ui::ImageModel::FromImageSkia(
          item_weak_ ? item_weak_->GetIcon(app_list_config_->type())
                     : gfx::ImageSkia()));
    }
  }

  const bool use_fallback_icon = ShouldUseFallbackIconImageModel();
  const ui::ImageModel& image_model =
      use_fallback_icon ? fallback_icon_image_model_ : icon_image_model_;
  if (!use_fallback_icon && !fallback_icon_image_model_.IsEmpty()) {
    fallback_icon_image_model_ = ui::ImageModel();
  }

  gfx::ImageSkia image_icon;
  if (image_model.IsImage()) {
    image_icon = image_model.GetImage().AsImageSkia();
  } else if (image_model.IsVectorIcon() && GetColorProvider()) {
    image_icon = ui::ThemedVectorIcon(image_model.GetVectorIcon())
                     .GetImageSkia(GetColorProvider());
  }

  if (features::IsSeparateWebAppShortcutBadgeIconEnabled()) {
    SetIconAndMaybeHostBadgeIcon(
        image_icon, update_item_icon ? item_weak_->GetHostBadgeIcon()
                                     : host_badge_icon_image_);
  } else {
    SetIconAndMaybeHostBadgeIcon(image_icon, gfx::ImageSkia());
  }
}

bool AppListItemView::ShouldUseFallbackIconImageModel() const {
  if (fallback_icon_image_model_.IsEmpty()) {
    return false;
  }

  if (prefer_fallback_icon_) {
    return true;
  }

  if (!item_weak_) {
    return true;
  }

  return item_weak_->GetMetadata()->is_placeholder_icon ||
         item_weak_->GetDefaultIcon().isNull();
}

void AppListItemView::SetIconAndMaybeHostBadgeIcon(
    const gfx::ImageSkia& icon,
    const gfx::ImageSkia& host_badge_icon) {
  // This function is used when AppListItem icons or host badge icons are used
  // for painting.
  DCHECK(use_item_icon_);

  // Clear icon and bail out if item icon is empty.
  if (icon.isNull()) {
    icon_->SetImage(nullptr);
    icon_image_model_ =
        ui::ImageModel(ui::ImageModel::FromImageSkia(gfx::ImageSkia()));
    return;
  }

  const gfx::Size icon_size =
      has_host_badge_
          ? gfx::ScaleToRoundedSize(app_list_config_->GetShortcutIconSize(),
                                    icon_scale_)
          : gfx::ScaleToRoundedSize(GetIconSize(), icon_scale_);

  icon_image_ = icon;

  host_badge_icon_image_ = has_host_badge_ ? host_badge_icon : gfx::ImageSkia();

  if (GetColorProvider() && !host_badge_icon_image_.isNull()) {
    icon_->SetImage(CreateBadgedShortcutImage(*app_list_config_, icon,
                                              host_badge_icon, icon_scale_,
                                              GetColorProvider()));
  } else {
    icon_->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        icon, skia::ImageOperations::RESIZE_BEST, icon_size));
  }

  DeprecatedLayoutImmediately();
}

gfx::Size AppListItemView::GetIconSize() const {
  if (is_folder_) {
    return app_list_config_->folder_icon_size();
  }
  if (is_promise_app_ && features::ArePromiseIconsEnabled() && item_weak_) {
    // Placeholder icons do not change size between states.
    if (ImageModelHasPlaceholderIcon()) {
      return gfx::Size(kPlaceholderIconDimension, kPlaceholderIconDimension);
    }
    return GetPreferredIconSizeForProgressRing();
  }

  return app_list_config_->grid_icon_size();
}

bool AppListItemView::ItemHasPlaceholderIcon() {
  return is_promise_app_ && item_weak_ &&
         item_weak_->GetMetadata()->is_placeholder_icon;
}

void AppListItemView::UpdateAppListConfig(
    const AppListConfig* app_list_config) {
  app_list_config_ = app_list_config;

  DCHECK(app_list_config_);

  views::InstallRoundRectHighlightPathGenerator(
      this, gfx::Insets(1), app_list_config_->grid_focus_corner_radius());

  if (!item_weak_ && use_item_icon_) {
    SetIconAndMaybeHostBadgeIcon(gfx::ImageSkia(), gfx::ImageSkia());
    return;
  }

  if (!use_item_icon_) {
    folder_icon_->UpdateAppListConfig(app_list_config);
  }
  title()->SetFontList(app_list_config_->app_title_font());
  UpdateIconView(/*update_item_icon=*/true);
  SetBackgroundExtendedState(is_icon_extended_, /*animate=*/false);
}

void AppListItemView::UpdateDraggedItem(const AppListItem* dragged_item) {
  if (!use_item_icon_) {
    folder_icon_->UpdateDraggedItem(dragged_item ? dragged_item->id() : "");
  }
}

gfx::Size AppListItemView::GetPreferredIconSizeForProgressRing() const {
  DCHECK(is_promise_app_ || ShouldUseFallbackIconImageModel());
  CHECK(item_weak_);

  if (ImageModelHasPlaceholderIcon()) {
    return gfx::Size(app_list_config_->promise_icon_dimension_pending(),
                     app_list_config_->promise_icon_dimension_pending());
  }

  switch (item_weak_->app_status()) {
    case AppStatus::kPending:
      return gfx::Size(app_list_config_->promise_icon_dimension_pending(),
                       app_list_config_->promise_icon_dimension_pending());
    case AppStatus::kInstalling:
    case AppStatus::kInstallCancelled:
    case AppStatus::kInstallSuccess:
    case AppStatus::kPaused:
      return gfx::Size(app_list_config_->promise_icon_dimension_installing(),
                       app_list_config_->promise_icon_dimension_installing());
    case AppStatus::kReady:
    case AppStatus::kBlocked:
      return app_list_config_->grid_icon_size();
  }
}

void AppListItemView::ScaleIconImmediatly(float scale_factor) {
  if (icon_scale_ == scale_factor) {
    return;
  }
  icon_scale_ = scale_factor;
  if (!use_item_icon_) {
    folder_icon_->SetIconScale(icon_scale_);
  }
  UpdateIconView(/*update_item_icon=*/false);
  layer()->SetTransform(gfx::Transform());
  if (progress_indicator_) {
    UpdateProgressRingBounds();
    progress_indicator_->layer()->SetTransform(gfx::Transform());
  }
}

void AppListItemView::UpdateBackgroundLayerBounds() {
  gfx::Rect background_bounds = GetIconView()->bounds();

  // Set icon bounds to it's max possible size - the background view will be
  // clipped to the required size as the background extended state gets updated.
  // This lets extended state animations run by updating the background layer
  // clip rect only (without having to change the icon background bounds at
  // different times depending on whether the background is shrinking or
  // expanding).
  int outset_from_icon =
      (app_list_config_->icon_extended_background_dimension() * icon_scale_ -
       background_bounds.width()) /
      2;
  background_bounds.Outset(outset_from_icon);
  icon_background_->SetBoundsRect(background_bounds);

  // Note that the background size should initially be the folder icon size
  // instead of the grid icon size. This is because the app icon has a
  // transparent ring around the visible icon which makes it look smaller.
  background_bounds.ClampToCenteredSize(gfx::ScaleToRoundedSize(
      app_list_config_->icon_visible_size(), icon_scale_));
  icon_background_->layer()->SetRoundedCornerRadius(
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
      if (ui_state_ == UI_STATE_DRAGGING ||
          ui_state_ == UI_STATE_TOUCH_DRAGGING) {
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
    case UI_STATE_TOUCH_DRAGGING:
      title_->SetVisible(false);
      if (new_install_dot_) {
        new_install_dot_->SetVisible(false);
      }
      ScaleAppIcon(false);
      break;
  }
  ui_state_ = ui_state;

  SchedulePaint();
}

void AppListItemView::ScaleAppIcon(bool scale_up) {
  // If there is no layer and the icon will scale down, avoid creating an
  // animation and just scale down.
  if (!layer() && !scale_up) {
    icon_scale_ = 1.0f;
    UpdateIconView(false);
    return;
  }

  EnsureLayer();

  if (scale_up) {
    icon_scale_ = kDragDropAppIconScale;
    UpdateIconView(false);
    const gfx::Transform scale_transform = gfx::GetScaleTransform(
        GetIconView()->bounds().CenterPoint(), 1 / kDragDropAppIconScale);
    layer()->SetTransform(scale_transform);
    if (progress_indicator_) {
      progress_indicator_->layer()->SetTransform(scale_transform);
    }
  } else if (drag_state_ != DragState::kNone) {
    // If a drag view has been created for this icon, the item transition to
    // target bounds is handled by the apps grid view bounds animator. At the
    // end of that animation, the layer will be destroyed, causing the
    // animation observer to get canceled. For this case, we need to scale
    // down the icon immediately, with no animation.
    ScaleIconImmediatly(1.0f);
  }

  ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::Milliseconds((kDragDropAppIconScaleTransitionInMs)));
  settings.SetTweenType(gfx::Tween::ACCEL_20_DECEL_100);
  if (scale_up) {
    layer()->SetTransform(gfx::Transform());
    if (progress_indicator_) {
      progress_indicator_->layer()->SetTransform(gfx::Transform());
    }
  } else {
    if (drag_state_ == DragState::kNone) {
      // To avoid poor quality icons, update icon image with the correct scale
      // after the transform animation is completed.
      settings.AddObserver(this);
      const gfx::Transform reverse_scale_transform = gfx::GetScaleTransform(
          GetContentsBounds().CenterPoint(), 1 / kDragDropAppIconScale);
      layer()->SetTransform(reverse_scale_transform);
      if (progress_indicator_) {
        progress_indicator_->layer()->SetTransform(reverse_scale_transform);
      }
    }
  }
}

void AppListItemView::OnImplicitAnimationsCompleted() {
  ScaleIconImmediatly(1.0f);
}

void AppListItemView::SetTouchDragging(bool touch_dragging) {
  if (mouse_dragging_ || touch_dragging_ == touch_dragging) {
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
  if (touch_dragging_ || mouse_dragging_ == mouse_dragging) {
    return;
  }

  mouse_dragging_ = mouse_dragging;

  if (mouse_dragging) {
    chromeos::haptics_util::PlayHapticTouchpadEffect(
        ui::HapticTouchpadEffect::kTick,
        ui::HapticTouchpadEffectStrength::kMedium);
  }

  SetState(STATE_NORMAL);
  SetUIState(mouse_dragging_ ? UI_STATE_DRAGGING : UI_STATE_NORMAL);
}

void AppListItemView::OnMouseDragTimer() {
  // Show scaled up app icon to indicate draggable state.
  SetMouseDragging(true);
}

void AppListItemView::OnTouchDragTimer(
    const gfx::Point& tap_down_location,
    const gfx::Point& tap_down_root_location) {
  // Show scaled up app icon to indicate draggable state.
  if (!InitiateDrag(tap_down_location, tap_down_root_location)) {
    return;
  }

  SetTouchDragging(true);
}

bool AppListItemView::InitiateDrag(const gfx::Point& location,
                                   const gfx::Point& root_location) {
  if (!IsItemDraggable()) {
    return false;
  }
  drag_state_ = DragState::kInitialized;
  SilentlyRequestFocus();
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

void AppListItemView::OnDragDone() {
  EnsureSelected();
  OnDragEnded();
}

void AppListItemView::ScrollRectToVisible(const gfx::Rect& rect) {
  gfx::Rect enlarged_rect = rect;
  enlarged_rect.Outset(8);

  views::Button::ScrollRectToVisible(enlarged_rect);
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

  // Use full name for accessibility.
  GetViewAccessibility().SetName(
      is_folder_ ? l10n_util::GetStringFUTF16(
                       IDS_APP_LIST_FOLDER_BUTTON_ACCESSIBILE_NAME,
                       full_name.empty() ? folder_name_placeholder : full_name)
                 : full_name);
  DeprecatedLayoutImmediately();
}

void AppListItemView::SetItemAccessibleName(const std::u16string& name) {
  GetViewAccessibility().SetName(name);
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
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view) {
    // Set `focus_removed_by_context_menu_` to restore focus when the context
    // menu closes. As an exception, do not restore focus on an inactive system
    // textfield (e.g. the folder name view).
    ash::SystemTextfield* as_system_textfield =
        views::AsViewClass<ash::SystemTextfield>(focused_view);
    focus_removed_by_context_menu_ =
        !as_system_textfield || as_system_textfield->IsActive();

    GetFocusManager()->ClearFocus();
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
    case Context::kAppsCollection:
      app_type =
          AppListMenuModelAdapter::PRODUCTIVITY_LAUNCHER_APPS_COLLECTIONS;
      metric_params.launched_from =
          AppListLaunchedFrom::kLaunchedFromAppsCollections;
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

  if (context_ == Context::kAppsCollection) {
    item_menu_model_adapter_ =
        std::make_unique<AppsCollectionsMenuModelAdapter>(
            item_weak_->GetMetadata()->id, std::move(menu_model), GetWidget(),
            source_type, metric_params, app_type,
            base::BindOnce(&AppListItemView::OnMenuClosed,
                           weak_ptr_factory_.GetWeakPtr()),
            view_delegate_->IsInTabletMode(), item_weak_->collection_id());

  } else {
    item_menu_model_adapter_ = std::make_unique<AppListMenuModelAdapter>(
        item_weak_->GetMetadata()->id, std::move(menu_model), GetWidget(),
        source_type, metric_params, app_type,
        base::BindOnce(&AppListItemView::OnMenuClosed,
                       weak_ptr_factory_.GetWeakPtr()),
        view_delegate_->IsInTabletMode(), item_weak_->collection_id());
  }

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

  // When the context menu comes from the apps grid or the apps collections grid
  // it has sorting options. When it comes from recent apps it has an option to
  // hide the continue section.
  AppListItemContext item_context;
  switch (context_) {
    case Context::kAppsGridView:
      item_context = AppListItemContext::kAppsGrid;
      break;
    case Context::kAppsCollection:
      item_context = AppListItemContext::kAppsCollectionsGrid;
      break;
    case Context::kRecentAppsView:
      item_context = AppListItemContext::kRecentApps;
      break;
  }
  view_delegate_->GetContextMenuModel(
      item_weak_->id(), item_context,
      base::BindOnce(&AppListItemView::OnContextMenuModelReceived,
                     weak_ptr_factory_.GetWeakPtr(), point, source_type));
}

bool AppListItemView::ShouldEnterPushedState(const ui::Event& event) {
  if (drag_state_ != DragState::kNone) {
    return false;
  }
  // Don't enter pushed state for EventType::kGestureTapDown so that hover gray
  // background does not show up during scroll.
  if (event.type() == ui::EventType::kGestureTapDown) {
    return false;
  }

  return views::Button::ShouldEnterPushedState(event);
}

bool AppListItemView::OnMousePressed(const ui::MouseEvent& event) {
  Button::OnMousePressed(event);
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

void AppListItemView::Layout(PassKey) {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    return;
  }

  views::FocusRing::Get(this)->DeprecatedLayoutImmediately();

  const gfx::Size icon_size = GetIconSize();

  const gfx::Rect icon_bounds = GetIconBoundsForTargetViewBounds(
      app_list_config_, rect, gfx::ScaleToRoundedSize(icon_size, icon_scale_),
      icon_scale_);

  GetIconView()->SetBoundsRect(icon_bounds);
  UpdateBackgroundLayerBounds();
  SetBackgroundExtendedState(is_icon_extended_, /*animate=*/false);

  gfx::Rect title_bounds = GetTitleBoundsForTargetViewBounds(
      app_list_config_, rect,
      title_->GetPreferredSize(views::SizeBounds(title_->width(), {})),
      icon_scale_);
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
      is_folder_ ? 0
                 : std::round(icon_bounds.width() *
                              kNotificationIndicatorPaddingRatio);

  const float indicator_x =
      icon_bounds.right() - indicator_size - indicator_padding;
  const float indicator_y = icon_bounds.y() + indicator_padding;

  const gfx::Rect indicator_bounds = gfx::ToRoundedRect(
      gfx::RectF(indicator_x, indicator_y, indicator_size, indicator_size));
  notification_indicator_->SetIndicatorBounds(indicator_bounds);

  if (progress_indicator_) {
    UpdateProgressRingBounds();
  }
}

gfx::Size AppListItemView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
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
  // Reset all states if we are already dragging, and avoid triggering a button
  // mouse release event.
  if (drag_state_ == DragState::kInitialized &&
      ui_state_ == UI_STATE_DRAGGING) {
    SetMouseDragging(false);
    drag_state_ = DragState::kNone;
    return;
  } else {
    // Triggers the button's click handler callback, which might delete `this`.
    Button::OnMouseReleased(event);
  }

  if (!weak_this) {
    return;
  }

  SetMouseDragging(false);

    // Cancel drag timer set when the mouse was pressed, to prevent the app
    // item from entering dragged state.
    mouse_drag_timer_.Stop();
    drag_state_ = DragState::kNone;
}

void AppListItemView::OnMouseCaptureLost() {
  Button::OnMouseCaptureLost();
  SetMouseDragging(false);
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
  if (!IsItemDraggable()) {
    return ui::DragDropTypes::DRAG_NONE;
  }

  return ui::DragDropTypes::DRAG_MOVE;
}

void AppListItemView::WriteDragData(const gfx::Point& press_pt,
                                    OSExchangeData* data) {
  if (item_weak_) {
    data->provider().SetDragImage(GetDragImage(), press_pt.OffsetFromOrigin());
    const DraggableAppType app_type = is_folder_
                                          ? DraggableAppType::kFolderAppGridItem
                                          : DraggableAppType::kAppGridItem;
    base::Pickle data_pickle;
    data_pickle.WriteString(item_weak_->id());
    data_pickle.WriteInt(static_cast<int>(app_type));
    data->SetPickledData(GetAppItemFormatType(), data_pickle);
  }
}

bool AppListItemView::MaybeStartTouchDrag(const gfx::Point& location) {
  int drag_operations = GetDragOperations(location);
  views::Widget* widget = GetWidget();
  DCHECK(widget);
  if (drag_operations == ui::DragDropTypes::DRAG_NONE ||
      widget->dragged_view()) {
    return false;
  }

  SetUIState(UI_STATE_TOUCH_DRAGGING);
  auto data = std::make_unique<ui::OSExchangeData>();
  WriteDragData(
      location - gfx::Vector2d(0, std::ceil(kTouchDragImageVerticalOffset /
                                            kDragDropAppIconScale)),
      data.get());

  gfx::Point widget_location(location);
  views::View::ConvertPointToWidget(this, &widget_location);
  widget->RunShellDrag(this, std::move(data), widget_location, drag_operations,
                       ui::mojom::DragEventSource::kTouch);
  return true;
}

void AppListItemView::OnGestureEvent(ui::GestureEvent* event) {
  gfx::Point screen_location(event->location());
  switch (event->type()) {
    case ui::EventType::kGestureScrollBegin:
      if (touch_dragging_) {
        OnDragStarted();
        event->SetHandled();
      } else {
        touch_drag_timer_.Stop();
      }
      break;
    case ui::EventType::kGestureScrollUpdate:
      if (touch_dragging_ && drag_state_ != DragState::kNone) {
        MaybeStartTouchDrag(event->location());
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureTapDown:
      if (GetState() != STATE_DISABLED && IsItemDraggable()) {
        SetState(STATE_PRESSED);
        touch_drag_timer_.Start(
            FROM_HERE, base::Milliseconds(kTouchLongpressDelayInMs),
            base::BindOnce(&AppListItemView::OnTouchDragTimer,
                           base::Unretained(this), event->location(),
                           event->root_location()));
        event->SetHandled();
      }
      break;
    case ui::EventType::kGestureTap:
    case ui::EventType::kGestureTapCancel:
      if (GetState() != STATE_DISABLED) {
        touch_drag_timer_.Stop();
        SetState(STATE_NORMAL);
      }
      break;
    case ui::EventType::kGestureLongTap:
    case ui::EventType::kGestureEnd:
      if (drag_state_ == DragState::kInitialized) {
        // Reset `drag_state_` if there was an attempt to initiate it (i.e. the
        // touch drag timer fired) but was not properly started (i.e. the app
        // item was never actually dragged) before a release event occurred.
        drag_state_ = DragState::kNone;
      }
      touch_drag_timer_.Stop();
      SetTouchDragging(false);
      if (IsShowingAppMenu()) {
        grid_delegate_->SetSelectedView(this);
      }
      break;
    case ui::EventType::kGestureLongPress:
      // Handle the long press event on long press to avoid RootView to
      // trigger View::DoDrag for this view before the item is dragged.
      View::ConvertPointToScreen(this, &screen_location);
      ShowContextMenu(screen_location, ui::MENU_SOURCE_TOUCH);
      event->SetHandled();
      break;
    case ui::EventType::kGestureTwoFingerTap:
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
    if (icon_background_) {
      icon_background_->layer()->SetColor(
          GetColorProvider()->GetColor(GetBackgroundLayerColorId()));
    }
  }

  UpdateIconView(/*update_item_icon=*/true);

  // Redraw progress indicator to adjust colors.
  if (progress_indicator_) {
    progress_indicator_->InvalidateLayer();
  }

  SchedulePaint();
}

std::u16string AppListItemView::GetTooltipText(const gfx::Point& p) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
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

bool AppListItemView::FireMouseDragTimerForTest() {
  if (!mouse_drag_timer_.IsRunning()) {
    return false;
  }

  mouse_drag_timer_.FireNow();
  return true;
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

bool AppListItemView::IsItemDraggable() const {
  return context_ == Context::kAppsGridView;
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
      app_list_config_, GetContentsBounds(),
      title_->GetPreferredSize(views::SizeBounds(title_->width(), {})),
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

void AppListItemView::ClearItemDraggingState() {
  SetState(STATE_NORMAL);
  SetMouseDragging(false);
  SetTouchDragging(false);
}

void AppListItemView::AnimateInFromPromiseApp(
    const ui::ImageModel& fallback_image,
    base::RepeatingClosure callback) {
  // Set up the app list item view so it appears as a promise icon - add a
  // progress ring (in completed state), scale the icon down, and hide the title
  // and the new install indicator.
  forced_progress_indicator_value_ = ProgressIndicator::kForcedShow;
  UpdateProgressIndicatorState();

  prefer_fallback_icon_ = true;
  fallback_icon_image_model_ = fallback_image;
  UpdateIconView(/*update_item_icon=*/false);

  views::View* const icon_view = GetIconView();
  icon_view->SetPaintToLayer();
  icon_view->layer()->SetFillsBoundsOpaquely(false);

  title_->SetPaintToLayer();
  title_->layer()->SetFillsBoundsOpaquely(false);
  title_->layer()->SetOpacity(0.0f);

  new_install_dot_->SetPaintToLayer();
  new_install_dot_->layer()->SetFillsBoundsOpaquely(false);
  new_install_dot_->layer()->SetOpacity(0.0f);

  const gfx::Point center_point = gfx::Rect(GetIconSize()).CenterPoint();
  const float starting_size =
      fallback_icon_image_model_.IsVectorIcon()
          ? kPlaceholderIconDimension
          : static_cast<float>(
                app_list_config_->promise_icon_dimension_installing());

  icon_view->layer()->SetTransform(gfx::GetScaleTransform(
      center_point,
      starting_size /
          static_cast<float>(app_list_config_->grid_icon_dimension())));

  // Animate the app list view out of the promise app state.
  views::AnimationBuilder animation;
  animation.OnEnded(base::BindOnce(&AppListItemView::OnAnimatedInFromPromiseApp,
                                   weak_ptr_factory_.GetWeakPtr(), callback));
  animation.OnAborted(
      base::BindOnce(&AppListItemView::OnAnimatedInFromPromiseApp,
                     weak_ptr_factory_.GetWeakPtr(), callback));
  animation.Once()
      .SetDuration(kSwapPromiseIconDuration)
      .SetOpacity(progress_indicator_->layer(), 0.0f,
                  gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetOpacity(title_->layer(), 1.0f, gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetOpacity(new_install_dot_->layer(), 1.0f,
                  gfx::Tween::FAST_OUT_LINEAR_IN)
      .SetTransform(icon_view->layer(), gfx::Transform(),
                    gfx::Tween::FAST_OUT_LINEAR_IN);
}

void AppListItemView::OnAnimatedInFromPromiseApp(
    base::RepeatingClosure callback) {
  title_->DestroyLayer();
  new_install_dot_->DestroyLayer();
  forced_progress_indicator_value_.reset();
  if (progress_indicator_ && layer()) {
    layer()->Remove(progress_indicator_->layer());
  }
  progress_indicator_.reset();
  // Clear background set as a result of adding progress indicator.
  SetBackground(nullptr);

  prefer_fallback_icon_ = false;

  if (!ShouldUseFallbackIconImageModel()) {
    fallback_icon_image_model_ = ui::ImageModel();
  }

  GetIconView()->DestroyLayer();
  UpdateIconView(/*update_item_icon=*/true);

  callback.Run();
}

std::optional<size_t> AppListItemView::item_counter_count_for_test() const {
  DCHECK(!use_item_icon_);
  return folder_icon_->GetItemCounterCount();
}

ProgressIndicator* AppListItemView::GetProgressIndicatorForTest() const {
  DCHECK(is_promise_app_);
  return progress_indicator_.get();
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

views::View* AppListItemView::GetIconView() const {
  if (use_item_icon_) {
    return icon_;
  }

  return folder_icon_;
}

gfx::Rect AppListItemView::GetIconBounds() const {
  return GetIconView()->bounds();
}

gfx::Rect AppListItemView::GetIconBoundsInScreen() const {
  gfx::Rect icon_bounds = GetIconBounds();
  ConvertRectToScreen(this, &icon_bounds);
  return icon_bounds;
}

gfx::ImageSkia AppListItemView::GetDragImage() const {
  if (!GetColorProvider() || !app_list_config_) {
    return gfx::ImageSkia();
  }

  if (is_folder_) {
    return folder_icon_->CreateDragImage();
  }
  if (has_host_badge_) {
    return CreateBadgedShortcutImage(*app_list_config_, icon_image_,
                                     host_badge_icon_image_,
                                     kDragDropAppIconScale, GetColorProvider());
  }
  return icon_->GetImage();
}

void AppListItemView::SetIconVisible(bool visible) {
  GetIconView()->SetVisible(visible);
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
  rect.Inset(gfx::Insets::TLBR(0, 0, config->grid_icon_bottom_padding(), 0));
  rect.ClampToCenteredSize(icon_size);
  return rect;
}

// static
gfx::Rect AppListItemView::GetHostBadgeIconBoundsForTargetViewBounds(
    const gfx::Rect& main_icon_bounds,
    const gfx::Size& host_badge_icon_with_background_size,
    const float icon_scale) {
  gfx::Rect rect(main_icon_bounds.CenterPoint(),
                 host_badge_icon_with_background_size);
  rect.ClampToCenteredSize(host_badge_icon_with_background_size);
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
  UpdateIconView(/*update_item_icon=*/true);
}

void AppListItemView::ItemNameChanged() {
  SetItemName(base::UTF8ToUTF16(item_weak_->name()),
              base::UTF8ToUTF16(item_weak_->GetAccessibleName()));
}

void AppListItemView::ItemHostBadgeIconChanged() {
  DCHECK(item_weak_);
  UpdateIconView(/*update_item_icon=*/true);
}

void AppListItemView::ItemBadgeVisibilityChanged() {
  if (GetIconView()) {
    notification_indicator_->SetVisible(item_weak_->has_notification_badge());
  }
}

void AppListItemView::ItemBadgeColorChanged() {
  notification_indicator_->SetColor(item_weak_->GetNotificationBadgeColor());
}

void AppListItemView::ItemIsNewInstallChanged() {
  DCHECK(item_weak_);
  if (new_install_dot_) {
    new_install_dot_->SetVisible(item_weak_->is_new_install());
    DeprecatedLayoutImmediately();
  }

  UpdateAccessibleDescription();
}

void AppListItemView::ItemBeingDestroyed() {
  DCHECK(item_weak_);
  item_weak_->RemoveObserver(this);
  item_weak_ = nullptr;
  UpdateAccessibleDescription();
  if (!use_item_icon_) {
    folder_icon_->ResetFolderItem();
  }
}

void AppListItemView::ItemProgressUpdated() {
  UpdateProgressIndicatorState();
}

void AppListItemView::ItemAppStatusUpdated() {
  UpdateProgressIndicatorState();
  UpdateAccessibleDescription();
}

void AppListItemView::ItemAppCollectionIdChanged() {
  UpdateAccessibleDescription();
}

bool AppListItemView::ImageModelHasPlaceholderIcon() const {
  return ShouldUseFallbackIconImageModel()
             ? fallback_icon_image_model_.IsVectorIcon()
             : icon_image_model_.IsVectorIcon();
}

void AppListItemView::UpdateProgressIndicatorState() {
  if ((!is_promise_app_ && !forced_progress_indicator_value_) ||
      !features::ArePromiseIconsEnabled()) {
    return;
  }

  if (!progress_indicator_) {
    progress_indicator_ =
        ProgressIndicator::CreateDefaultInstance(base::BindRepeating(
            [](AppListItemView* view) -> std::optional<float> {
              if (view->forced_progress_indicator_value_) {
                return *view->forced_progress_indicator_value_;
              }
              if (view->item()->app_status() == AppStatus::kPending) {
                return 0.0f;
              }
              // If download is in-progress, return the progress as a decimal.
              // Otherwise, the progress indicator shouldn't be painted.
              float progress = view->item()->GetMetadata()->progress;
              return (progress >= 0.f && progress < 1.f)
                         ? progress
                         : ProgressIndicator::kProgressComplete;
            },
            base::Unretained(this)));
    progress_indicator_->SetInnerIconVisible(false);
    progress_indicator_->SetInnerRingVisible(false);
    progress_indicator_->SetOuterRingStrokeWidth(
        static_cast<float>(kPromiseRingStrokeSize));
    EnsureLayer();
    layer()->Add(progress_indicator_->CreateLayer(base::BindRepeating(
        [](AppListItemView* view, ui::ColorId color_id) {
          return view->GetColorProvider()->GetColor(color_id);
        },
        base::Unretained(this))));
  }

  EnsureLayer();

  if (item()->app_status() == AppStatus::kPending) {
    progress_indicator_->SetColorId(cros_tokens::kCrosSysHighlightShape);
    progress_indicator_->SetOuterRingTrackVisible(true);
  } else {
    progress_indicator_->SetColorId(cros_tokens::kCrosSysPrimary);
    progress_indicator_->SetOuterRingTrackVisible(false);
  }

  UpdateProgressRingBounds();
}

void AppListItemView::UpdateProgressRingBounds() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty()) {
    return;
  }

  CHECK(!is_folder_);

  gfx::Rect progress_bounds = gfx::Rect(
      views::View::ConvertRectToTarget(icon_, this, icon_->GetImageBounds()));

  const gfx::Size promise_icon_preferred_size = gfx::ScaleToRoundedSize(
      GetPreferredIconSizeForProgressRing(), icon_scale_);

  // If the icon is smaller than the expected icon size (i,e for placeholder
  // icons), add padding to ensure the overall size of the promise icon is
  // correct regardless of the image icon size.
  progress_bounds.Outset(gfx::Outsets::VH(
      std::max(
          0,
          (promise_icon_preferred_size.width() - progress_bounds.width()) / 2),
      std::max(
          0, (promise_icon_preferred_size.height() - progress_bounds.height()) /
                 2)));

  const gfx::Insets progress_ring_padding =
      ImageModelHasPlaceholderIcon() ||
              item()->app_status() == AppStatus::kPending
          ? kProgressRingMarginPending
          : kProgressRingMarginInstalling;

  progress_bounds.Inset(progress_ring_padding);

  // The Progress indicator paints the ring within the bounds of the layer, so
  // add padding for the promise ring.
  progress_bounds.Inset(-gfx::Insets(kPromiseRingStrokeSize));

  // The masked icons include 1px padding.
  progress_bounds.Inset(1);

  progress_indicator_->layer()->SetBounds(progress_bounds);

  layer()->StackAtBottom(progress_indicator_->layer());
  progress_indicator_->InvalidateLayer();

  SetBackground(std::make_unique<PromiseIconBackground>(
      cros_tokens::kCrosSysSystemOnBase, progress_bounds,
      progress_ring_padding));
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
  icon_background_->SetVisible(true);
  GetIconView()->SetPaintToLayer();
  GetIconView()->layer()->SetFillsBoundsOpaquely(false);

  base::AutoReset<bool> auto_reset(&setting_up_icon_animation_, true);
  ui::Layer* const background_layer = GetIconBackgroundLayer();
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

  UpdateBackgroundLayerBounds();
  const int width = extend_icon
                        ? app_list_config_->icon_extended_background_dimension()
                        : app_list_config_->icon_visible_dimension();
  gfx::Rect clip_rect(background_layer->size());
  clip_rect.ClampToCenteredSize(
      ScaleToRoundedSize(gfx::Size(width, width), icon_scale_));

  const int corner_radius =
      extend_icon ? app_list_config_->icon_extended_background_radius()
                  : width / 2;
  const base::TimeDelta duration =
      animate ? base::Milliseconds(125) : base::TimeDelta();
  builder.GetCurrentSequence()
      .SetDuration(duration)
      .SetClipRect(background_layer, clip_rect, animation_tween_type)
      .SetRoundedCorners(background_layer,
                         gfx::RoundedCornersF(corner_radius * icon_scale_),
                         animation_tween_type);
  if (GetWidget()) {
    builder.GetCurrentSequence().SetColor(
        background_layer,
        GetColorProvider()->GetColor(GetBackgroundLayerColorId()),
        animation_tween_type);
  }
}

ui::ColorId AppListItemView::GetBackgroundLayerColorId() const {
  if (is_icon_extended_) {
    return cros_tokens::kCrosSysRippleNeutralOnSubtle;
  }

  if (is_folder_) {
    return cros_tokens::kCrosSysSystemOnBase;
  }

  return cros_tokens::kCrosSysRippleNeutralOnSubtle;
}

void AppListItemView::OnExtendingAnimationEnded(bool extend_icon) {
  if (!setting_up_icon_animation_ && !extend_icon && !is_folder_) {
    icon_background_->SetVisible(false);
    GetIconView()->DestroyLayer();
  }
}

ui::Layer* AppListItemView::GetIconBackgroundLayer() {
  if (!icon_background_) {
    return nullptr;
  }
  return icon_background_->layer();
}

bool AppListItemView::AlwaysPaintsToLayer() {
  return is_promise_app_ || progress_indicator_;
}

void AppListItemView::UpdateAccessibleDescription() {
  if (!item_weak_) {
    GetViewAccessibility().RemoveDescription();
    return;
  }

  // The list of descriptions to be announced.
  std::vector<std::u16string> descriptions;

  if (item_weak_->is_folder()) {
    // For folder items, announce the number of apps in the folder.
    std::u16string app_count_announcement = l10n_util::GetPluralStringFUTF16(
        IDS_APP_LIST_FOLDER_NUMBER_OF_APPS_ACCESSIBILE_DESCRIPTION,
        item_weak_->AsFolderItem()->ChildItemCount());
    descriptions.push_back(app_count_announcement);
  }

  auto app_status = item_weak_->app_status();
  std::u16string app_status_description;
  switch (app_status) {
    case AppStatus::kBlocked:
      app_status_description =
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_BLOCKED_APP);
      break;
    case AppStatus::kPaused:
      app_status_description =
          ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
              IDS_APP_LIST_PAUSED_APP);
      break;
    default:
      if (item_weak_->is_new_install()) {
        app_status_description =
            ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                IDS_APP_LIST_NEW_INSTALL_ACCESSIBILE_DESCRIPTION);
      }
      break;
  }
  if (!app_status_description.empty()) {
    descriptions.push_back(app_status_description);
  }

  if (context_ == Context::kAppsCollection) {
    descriptions.push_back(GetAppCollectionName(item_weak_->collection_id()));
  }

  // Set the concatenated descriptions.
  if (!descriptions.empty()) {
    GetViewAccessibility().SetDescription(base::JoinString(descriptions, u" "));
  } else {
    GetViewAccessibility().RemoveDescription();
  }
}

BEGIN_METADATA(AppListItemView)
END_METADATA

}  // namespace ash
