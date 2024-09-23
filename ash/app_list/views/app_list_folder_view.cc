// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/app_list_folder_view.h"

#include <algorithm>
#include <limits>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_util.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_a11y_announcer.h"
#include "ash/app_list/views/app_list_folder_controller.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/folder_header_view.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/app_list/views/top_icon_animation_view.h"
#include "ash/controls/rounded_scroll_bar.h"
#include "ash/controls/scroll_view_gradient_helper.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/system_shadow.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/animation_delegate_views.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr int kFolderHeaderPadding = 12;
constexpr int kOnscreenKeyboardTopPadding = 16;

constexpr int kTileSpacingInFolder = 8;

constexpr int kScrollViewGradientSize = 16;

constexpr int kFolderBackgroundRadius = 12;

// Insets for the vertical scroll bar. The top is pushed down slightly to align
// with the icons, which keeps the scroll bar out of the rounded corner area.
constexpr auto kVerticalScrollInsets =
    gfx::Insets::TLBR(kTileSpacingInFolder, 0, 1, 1);

// Duration for fading in the target page when opening
// or closing a folder, and the duration for the top folder icon animation
// for flying in or out the folder.
constexpr base::TimeDelta kFolderTransitionDuration = base::Milliseconds(250);

// Returns true if ChromeVox (spoken feedback) is enabled.
bool IsSpokenFeedbackEnabled() {
  return Shell::HasInstance() &&  // May be null in tests.
         Shell::Get()->accessibility_controller()->spoken_feedback().enabled();
}

// Transit from the background of the folder item's icon to the opened
// folder's background when opening the folder. Transit the other way when
// closing the folder.
class BackgroundAnimation : public AppListFolderView::Animation,
                            public ui::ImplicitAnimationObserver,
                            public views::ViewObserver {
 public:
  BackgroundAnimation(bool show,
                      AppListFolderView* folder_view,
                      views::View* animating_view)
      : show_(show),
        folder_view_(folder_view),
        animating_view_(animating_view),
        shadow_(folder_view->shadow()) {
    background_view_observer_.Observe(animating_view_.get());
  }

  BackgroundAnimation(const BackgroundAnimation&) = delete;
  BackgroundAnimation& operator=(const BackgroundAnimation&) = delete;

  ~BackgroundAnimation() override = default;

 private:
  // AppListFolderView::Animation:
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    // Calculate the source and target states.
    const int icon_radius =
        folder_view_->GetAppListConfig()->folder_icon_radius();
    const int from_radius = show_ ? icon_radius : kFolderBackgroundRadius;
    const int to_radius = show_ ? kFolderBackgroundRadius : icon_radius;
    gfx::Rect from_rect = show_ ? folder_view_->folder_item_icon_bounds()
                                : animating_view_->bounds();
    from_rect -= animating_view_->bounds().OffsetFromOrigin();
    gfx::Rect to_rect = show_ ? animating_view_->bounds()
                              : folder_view_->folder_item_icon_bounds();
    to_rect -= animating_view_->bounds().OffsetFromOrigin();
    const views::Widget* app_list_widget = folder_view_->GetWidget();
    const SkColor background_color =
        app_list_widget->GetColorProvider()->GetColor(
            cros_tokens::kCrosSysSystemBaseElevated);
    const SkColor bubble_color = app_list_widget->GetColorProvider()->GetColor(
        cros_tokens::kCrosSysSystemOnBase);
    const SkColor from_color = show_ ? bubble_color : background_color;
    const SkColor to_color = show_ ? background_color : bubble_color;

    animating_view_->layer()->SetColor(from_color);
    animating_view_->layer()->SetClipRect(from_rect);
    animating_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(from_radius));

    AlignShadowWithAnimatingBackground();

    ui::ScopedLayerAnimationSettings settings(
        animating_view_->layer()->GetAnimator());
    settings.SetTransitionDuration(kFolderTransitionDuration);
    settings.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    settings.AddObserver(this);
    animating_view_->layer()->SetColor(to_color);
    animating_view_->layer()->SetClipRect(to_rect);
    animating_view_->layer()->SetRoundedCornerRadius(
        gfx::RoundedCornersF(to_radius));
    is_animating_ = true;
  }

  bool IsAnimationRunning() override { return is_animating_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    is_animating_ = false;

    folder_view_->RecordAnimationSmoothness();

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

  // views::ViewObserver:
  void OnViewLayerClipRectChanged(views::View* observed_view) override {
    // Shadow is painted on the nine patch layer according to its owner's shape,
    // so the shadow cannot be animated with the change of shadow layer's
    // attributes. We need to use the intermediate clip rect shape from
    // background animation to update shadow's contents bounds and corner
    // radius.
    DCHECK_EQ(observed_view, animating_view_);

    AlignShadowWithAnimatingBackground();
  }

  void AlignShadowWithAnimatingBackground() {
    // If layer clip rect is not empty, we use the clip rect to update the
    // shadow's contents bounds. Otherwise, we use the layer bounds.
    const auto* background_layer = animating_view_->layer();
    const gfx::Rect& background_bounds = background_layer->bounds();
    const gfx::Rect& clip_rect = background_layer->clip_rect();
    const gfx::Rect& content_bounds =
        clip_rect.IsEmpty() ? background_bounds
                            : clip_rect + background_bounds.OffsetFromOrigin();
    shadow_->SetContentBounds(content_bounds);
    shadow_->SetRoundedCornerRadius(
        background_layer->rounded_corner_radii().upper_left());
  }

  // True if opening the folder.
  const bool show_;
  bool is_animating_ = false;

  const raw_ptr<AppListFolderView> folder_view_;
  const raw_ptr<views::View> animating_view_;
  const raw_ptr<SystemShadow> shadow_;

  // Observes the rect clip change of background view.
  base::ScopedObservation<views::View, views::ViewObserver>
      background_view_observer_{this};

  base::OnceClosure completion_callback_;
};

// Decrease the opacity of the folder item's title when opening the folder.
// Increase it when closing the folder.
class FolderItemTitleAnimation : public AppListFolderView::Animation {
 public:
  FolderItemTitleAnimation(bool show,
                           AppListFolderView* folder_view,
                           views::View* folder_title)
      : show_(show), folder_view_(folder_view), folder_title_(folder_title) {}

  FolderItemTitleAnimation(const FolderItemTitleAnimation&) = delete;
  FolderItemTitleAnimation& operator=(const FolderItemTitleAnimation&) = delete;

  ~FolderItemTitleAnimation() override = default;

 private:
  // AppListFolderView::Animation:
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    views::AnimationBuilder()
        .OnEnded(base::BindOnce(&FolderItemTitleAnimation::AnimationEnded,
                                weak_ptr_factory_.GetWeakPtr()))
        .OnAborted(base::BindOnce(&FolderItemTitleAnimation::AnimationEnded,
                                  weak_ptr_factory_.GetWeakPtr()))
        .Once()
        .SetDuration(kFolderTransitionDuration)
        .SetOpacity(folder_title_->layer(), show_ ? 0.0f : 1.0f,
                    gfx::Tween::FAST_OUT_SLOW_IN);
  }
  bool IsAnimationRunning() override { return !!completion_callback_; }

  void AnimationEnded() {
    folder_view_->RecordAnimationSmoothness();

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

  const bool show_;

  const raw_ptr<AppListFolderView> folder_view_;  // Not owned.

  const raw_ptr<views::View, DanglingUntriaged> folder_title_;

  base::OnceClosure completion_callback_;

  base::WeakPtrFactory<FolderItemTitleAnimation> weak_ptr_factory_{this};
};

// Transit from the items within the folder item icon to the same items in the
// opened folder when opening the folder. Transit the other way when closing the
// folder.
class TopIconAnimation : public AppListFolderView::Animation,
                         public TopIconAnimationObserver {
 public:
  // The item view bounds such that the item icon matches the bounds of the icon
  // within the folder icon. The icon position within the app list item view
  // depends on whether the app item is badge or not.
  struct TopItemViewBounds {
    // Item view bounds for non-badge icon.
    const gfx::Rect not_badged;
    // Item view bounds for badged icon.
    const gfx::Rect badged;
  };

  TopIconAnimation(bool show,
                   AppListFolderView* folder_view,
                   views::ScrollView* scroll_view,
                   AppListItemView* folder_item_view)
      : show_(show),
        folder_view_(folder_view),
        scroll_view_(scroll_view),
        folder_item_view_(folder_item_view) {}

  TopIconAnimation(const TopIconAnimation&) = delete;
  TopIconAnimation& operator=(const TopIconAnimation&) = delete;

  ~TopIconAnimation() override {
    for (ash::TopIconAnimationView* view : top_icon_views_) {
      view->RemoveObserver(this);
    }
    top_icon_views_.clear();
  }

  // AppListFolderView::Animation
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    // Hide the original items in the folder until the animation ends.
    SetFirstPageItemViewsVisible(false);
    folder_item_view_->SetIconVisible(false);

    // Calculate the start and end bounds of the top item icons in the
    // animation.
    std::vector<TopItemViewBounds> top_item_views_bounds =
        GetTopItemViewsBoundsInFolderIcon();
    std::vector<gfx::Rect> first_page_item_views_bounds =
        GetFirstPageItemViewsBounds();
    top_icon_views_.clear();

    const AppListConfigType app_list_config_type =
        folder_view_->GetAppListConfig()->type();

    // Get top folder items that should be animated - note that item index in
    // the folder item list may not match the intended item bounds in
    // `first_page_item_views_bounds` if it's preceded by a drag item in the
    // item list.
    std::vector<const AppListItem*> top_items;
    const AppListItem* drag_item = folder_view_->items_grid_view()->drag_item();
    const AppListItemList* folder_items =
        folder_view_->folder_item()->item_list();
    for (size_t i = 0; i < folder_items->item_count(); ++i) {
      if (top_items.size() == first_page_item_views_bounds.size())
        break;
      const AppListItem* top_item = folder_items->item_at(i);
      if (top_item->GetIcon(app_list_config_type).isNull() ||
          top_item == drag_item) {
        // The item being dragged should be excluded.
        continue;
      }
      top_items.push_back(top_item);
    }

    for (size_t i = 0; i < top_items.size(); ++i) {
      const AppListItem* top_item = top_items[i];
      bool item_in_folder_icon = i < top_item_views_bounds.size();
      gfx::Rect scaled_rect;
      if (item_in_folder_icon) {
        if (top_item->GetHostBadgeIcon().isNull()) {
          scaled_rect = top_item_views_bounds[i].not_badged;
        } else {
          scaled_rect = top_item_views_bounds[i].badged;
        }
      } else {
        scaled_rect = folder_view_->folder_item_icon_bounds();
      }

      auto icon_view = std::make_unique<TopIconAnimationView>(
          folder_view_->items_grid_view(),
          top_item->GetIcon(app_list_config_type), top_item->GetHostBadgeIcon(),
          base::UTF8ToUTF16(top_item->GetDisplayName()), scaled_rect, show_,
          item_in_folder_icon);
      auto* icon_view_ptr = icon_view.get();

      icon_view_ptr->AddObserver(this);
      // Add the transitional views into child views, and set its bounds to the
      // same location of the item in the folder list view.
      top_icon_views_.push_back(
          folder_view_->animating_background()->AddChildView(
              std::move(icon_view)));
      icon_view_ptr->SetBoundsRect(first_page_item_views_bounds[i]);
      icon_view_ptr->TransformView(kFolderTransitionDuration);
    }

    if (top_icon_views_.empty())
      OnAnimationComplete();
  }

  bool IsAnimationRunning() override { return !top_icon_views_.empty(); }

  // TopIconAnimationObserver
  void OnTopIconAnimationsComplete(TopIconAnimationView* view) override {
    // Clean up the transitional view for which the animation completes.
    view->RemoveObserver(this);
    auto to_delete = base::ranges::find(top_icon_views_, view);
    DCHECK(to_delete != top_icon_views_.end());
    top_icon_views_.erase(to_delete);

    folder_view_->RecordAnimationSmoothness();

    // An empty list indicates that all animations are done.
    if (top_icon_views_.empty())
      OnAnimationComplete();
  }

  // Called when all top icon animations complete.
  void OnAnimationComplete() {
    // Set top item views visible when opening the folder.
    if (show_)
      SetFirstPageItemViewsVisible(true);

    // Show the folder icon when closing the folder.
    if (!show_)
      folder_item_view_->SetIconVisible(true);

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

 private:
  std::vector<TopItemViewBounds> GetTopItemViewsBoundsInFolderIcon() {
    const AppListConfig* const app_list_config =
        folder_view_->GetAppListConfig();
    size_t effective_folder_size =
        folder_view_->folder_item()->ChildItemCount();
    // If a folder item is being dragged, it should be hidden from the folder
    // item icon, and top icons bounds should be calculated as if the item is
    // not in the folder.
    if (folder_view_->items_grid_view()->drag_item())
      effective_folder_size -= 1;

    std::vector<gfx::Rect> top_icons_bounds = FolderImage::GetTopIconsBounds(
        *app_list_config, folder_view_->folder_item_icon_bounds(),
        std::min(effective_folder_size, FolderImage::kNumFolderTopItems));

    std::vector<TopItemViewBounds> top_item_views_bounds;
    const int not_badged_icon_dimension =
        app_list_config->grid_icon_dimension();
    const int badged_icon_dimension =
        app_list_config->GetShortcutIconSize().width();
    const int icon_bottom_padding = app_list_config->grid_icon_bottom_padding();
    const int tile_width = app_list_config->grid_tile_width();
    const int tile_height = app_list_config->grid_tile_height();

    auto get_item_bounds = [&](const gfx::Rect& target_icon_bounds,
                               int icon_dimension) {
      // Calculate the item view's bounds based on the icon bounds.
      gfx::Rect item_bounds(
          (icon_dimension - tile_width) / 2,
          (icon_dimension + icon_bottom_padding - tile_height) / 2, tile_width,
          tile_height);
      item_bounds = gfx::ScaleToRoundedRect(
          item_bounds,
          target_icon_bounds.width() / static_cast<float>(icon_dimension),
          target_icon_bounds.height() / static_cast<float>(icon_dimension));
      item_bounds.Offset(target_icon_bounds.x(), target_icon_bounds.y());
      return item_bounds;
    };

    for (gfx::Rect bounds : top_icons_bounds) {
      top_item_views_bounds.push_back(
          {.not_badged = get_item_bounds(bounds, not_badged_icon_dimension),
           .badged = get_item_bounds(bounds, badged_icon_dimension)});
    }
    return top_item_views_bounds;
  }

  void SetFirstPageItemViewsVisible(bool visible) {
    // Items grid view has to be visible in case an item is being reparented, so
    // only set the opacity here.
    folder_view_->items_grid_view()->layer()->SetOpacity(visible ? 1.0f : 0.0f);
    SetViewIgnoredForAccessibility(folder_view_->items_grid_view(), !visible);
  }

  // Get the bounds of the items in the first page of the opened folder relative
  // to AppListFolderView.
  std::vector<gfx::Rect> GetFirstPageItemViewsBounds() {
    std::vector<gfx::Rect> items_bounds;
    // Go over items in the folder, and collect bounds of items that fit within
    // the bounds of the first "page" of apps.
    const size_t count = folder_view_->folder_item()->ChildItemCount();
    const gfx::RectF container_bounds(scroll_view_->GetLocalBounds());
    for (size_t i = 0; i < count; ++i) {
      views::View* item = folder_view_->items_grid_view()->GetItemViewAt(i);
      if (folder_view_->items_grid_view()->IsViewHiddenForDrag(item))
        continue;

      // Stop if the item bounds are not within the container bounds - assumes
      // that subsequent item bounds would not be within the container view
      // either.
      gfx::RectF bounds_in_container(item->GetLocalBounds());
      views::View::ConvertRectToTarget(item, scroll_view_,
                                       &bounds_in_container);
      if (!container_bounds.Intersects(bounds_in_container)) {
        break;
      }

      // Return the item bounds in AppListFolderView coordinates.
      gfx::RectF bounds_in_folder(item->GetLocalBounds());
      views::View::ConvertRectToTarget(item, folder_view_, &bounds_in_folder);
      items_bounds.emplace_back(
          folder_view_->GetMirroredRect(gfx::ToRoundedRect(bounds_in_folder)));
    }
    return items_bounds;
  }

  // True if opening the folder.
  const bool show_;

  const raw_ptr<AppListFolderView> folder_view_;  // Not owned.

  // The scroll view that contains the apps grid.
  const raw_ptr<views::ScrollView> scroll_view_;

  // The app list item view with which the folder view is associated.
  // NOTE: Users of `TopIconAnimation` should ensure the animation does
  // not outlive the `folder_item_view_`.
  const raw_ptr<AppListItemView> folder_item_view_;

  std::vector<raw_ptr<TopIconAnimationView, VectorExperimental>>
      top_icon_views_;

  base::OnceClosure completion_callback_;
};

// Transit from the bounds of the folder item icon to the opened folder's
// bounds and transit opacity from 0 to 1 when opening the folder. Transit the
// other way when closing the folder.
class ContentsContainerAnimation : public AppListFolderView::Animation,
                                   public ui::ImplicitAnimationObserver {
 public:
  ContentsContainerAnimation(bool show,
                             bool hide_for_reparent,
                             AppListFolderView* folder_view)
      : show_(show),
        hide_for_reparent_(hide_for_reparent),
        folder_view_(folder_view) {}

  ContentsContainerAnimation(const ContentsContainerAnimation&) = delete;
  ContentsContainerAnimation& operator=(const ContentsContainerAnimation&) =
      delete;

  ~ContentsContainerAnimation() override { StopObservingImplicitAnimations(); }

  // AppListFolderView::Animation
  void ScheduleAnimation(base::OnceClosure completion_callback) override {
    DCHECK(!completion_callback_);
    completion_callback_ = std::move(completion_callback);

    // Transform used to scale the folder's contents container from the bounds
    // of the folder icon to that of the opened folder.
    gfx::Transform transform;
    const gfx::Rect scaled_rect(folder_view_->folder_item_icon_bounds());
    const gfx::Rect rect(folder_view_->contents_container()->bounds());
    ui::Layer* layer = folder_view_->contents_container()->layer();
    transform.Translate(scaled_rect.x() - rect.x(), scaled_rect.y() - rect.y());
    transform.Scale(static_cast<double>(scaled_rect.width()) / rect.width(),
                    static_cast<double>(scaled_rect.height()) / rect.height());

    if (show_)
      layer->SetTransform(transform);
    layer->SetOpacity(show_ ? 0.0f : 1.0f);

    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
    animation.AddObserver(this);
    animation.SetTransitionDuration(kFolderTransitionDuration);
    layer->SetTransform(show_ ? gfx::Transform() : transform);
    layer->SetOpacity(show_ ? 1.0f : 0.0f);

    is_animation_running_ = true;
  }

  bool IsAnimationRunning() override { return is_animation_running_; }

  // ui::ImplicitAnimationObserver
  void OnImplicitAnimationsCompleted() override {
    is_animation_running_ = false;

    // If the view is hidden for reparenting a folder item, it has to be
    // visible, so that drag_view_ can keep receiving mouse events.
    if (!show_ && !hide_for_reparent_)
      folder_view_->SetVisible(false);

    // Set the view bounds offscreen, so that it won't overlap the root level
    // apps grid view during folder item reparenting transitional period.
    // Keeping the same width and height avoids re-layout and ensures that
    // AppListItemView continues to receive events. The view will be set
    // invisible at the end of the drag.
    if (hide_for_reparent_) {
      const gfx::Rect& bounds = folder_view_->bounds();
      folder_view_->SetPosition(gfx::Point(-bounds.width(), -bounds.height()));
    }

    // Reset the transform after animation so that the following folder's
    // preferred bounds is calculated correctly.
    folder_view_->contents_container()->layer()->SetTransform(gfx::Transform());
    folder_view_->RecordAnimationSmoothness();

    if (completion_callback_)
      std::move(completion_callback_).Run();
  }

 private:
  // True if opening the folder.
  const bool show_;

  // True if an item in the folder is being reparented to root grid view.
  const bool hide_for_reparent_;

  const raw_ptr<AppListFolderView> folder_view_;

  bool is_animation_running_ = false;

  base::OnceClosure completion_callback_;
};

// ScrollViewWithMaxHeight limits its preferred size to a maximum height that
// shows 4 apps grid rows.
class ScrollViewWithMaxHeight : public views::ScrollView {
  METADATA_HEADER(ScrollViewWithMaxHeight, views::ScrollView)

 public:
  explicit ScrollViewWithMaxHeight(AppListFolderView* folder_view)
      : views::ScrollView(views::ScrollView::ScrollWithLayers::kEnabled),
        folder_view_(folder_view) {}
  ScrollViewWithMaxHeight(const ScrollViewWithMaxHeight&) = delete;
  ScrollViewWithMaxHeight& operator=(const ScrollViewWithMaxHeight&) = delete;
  ~ScrollViewWithMaxHeight() override = default;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    gfx::Size size = views::ScrollView::CalculatePreferredSize(available_size);
    const int tile_height =
        folder_view_->items_grid_view()->GetTotalTileSize(/*page=*/0).height();
    // Show a maximum of 4 full rows, plus a little bit of the next row to make
    // it obvious the view can scroll.
    const int max_height = (tile_height * 4) + (tile_height / 4);
    size.set_height(std::min(size.height(), max_height));
    return size;
  }

 private:
  const raw_ptr<AppListFolderView> folder_view_;
};

BEGIN_METADATA(ScrollViewWithMaxHeight)
END_METADATA

}  // namespace

AppListFolderView::AppListFolderView(AppListFolderController* folder_controller,
                                     AppsGridView* root_apps_grid_view,
                                     AppListA11yAnnouncer* a11y_announcer,
                                     AppListViewDelegate* view_delegate,
                                     bool tablet_mode)
    : folder_controller_(folder_controller),
      root_apps_grid_view_(root_apps_grid_view),
      a11y_announcer_(a11y_announcer),
      view_delegate_(view_delegate) {
  DCHECK(folder_controller_);
  DCHECK(root_apps_grid_view_);
  DCHECK(a11y_announcer_);
  DCHECK(view_delegate_);
  SetLayoutManager(std::make_unique<views::FillLayout>());

  // The background's corner radius cannot be changed in the same layer of the
  // contents container using layer animation, so use another layer to perform
  // such changes.
  background_view_ = AddChildView(std::make_unique<views::View>());
  background_view_->SetPaintToLayer(ui::LAYER_TEXTURED);
  background_view_->layer()->SetFillsBoundsOpaquely(false);
  background_view_->layer()->SetBackgroundBlur(
      ColorProvider::kBackgroundBlurSigma);
  background_view_->layer()->SetBackdropFilterQuality(
      ColorProvider::kBackgroundBlurQuality);
  background_view_->layer()->SetRoundedCornerRadius(
      gfx::RoundedCornersF(kFolderBackgroundRadius));
  background_view_->layer()->SetIsFastRoundedCorner(true);
  background_view_->SetBorder(std::make_unique<views::HighlightBorder>(
      kFolderBackgroundRadius,
      views::HighlightBorder::Type::kHighlightBorderOnShadow));
  background_view_->SetBackground(views::CreateThemedSolidBackground(
      cros_tokens::kCrosSysSystemBaseElevated));
  background_view_->SetVisible(false);

  animating_background_ = AddChildView(std::make_unique<views::View>());
  animating_background_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  animating_background_->layer()->SetBackgroundBlur(
      ColorProvider::kBackgroundBlurSigma);
  animating_background_->layer()->SetBackdropFilterQuality(
      ColorProvider::kBackgroundBlurQuality);
  animating_background_->layer()->SetFillsBoundsOpaquely(false);
  animating_background_->SetVisible(false);

  contents_container_ = AddChildView(std::make_unique<views::View>());
  contents_container_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);

  CreateScrollableAppsGrid(tablet_mode);

  // Create a shadow under `background_view_`.
  shadow_ = SystemShadow::CreateShadowOnNinePatchLayer(
      SystemShadow::Type::kElevation12,
      base::BindRepeating(&AppListFolderView::OnShadowLayerRecreated,
                          base::Unretained(this)));
  background_view_->AddLayerToRegion(shadow_->GetLayer(),
                                     views::LayerRegion::kBelow);

  AppListModelProvider::Get()->AddObserver(this);

  GetViewAccessibility().SetRole(ax::mojom::Role::kGenericContainer);
  UpdateExpandedCollapsedAccessibleState();
}

void AppListFolderView::CreateScrollableAppsGrid(bool tablet_mode) {
  // The top part of the folder contents is a scrollable apps grid.
  scroll_view_ = contents_container_->AddChildView(
      std::make_unique<ScrollViewWithMaxHeight>(this));
  scroll_view_->ClipHeightTo(0, std::numeric_limits<int>::max());
  scroll_view_->SetDrawOverflowIndicator(false);
  // Don't paint a background. The folder already has one.
  scroll_view_->SetBackgroundColor(std::nullopt);
  // Arrow keys are used to select app icons.
  scroll_view_->SetAllowKeyboardScrolling(false);

  // Set up fade in/fade out gradients at top/bottom of scroll view.
  scroll_view_->SetPaintToLayer(ui::LAYER_NOT_DRAWN);
  gradient_helper_ = std::make_unique<ScrollViewGradientHelper>(
      scroll_view_, kScrollViewGradientSize);

  // Set up scroll bars.
  scroll_view_->SetHorizontalScrollBarMode(
      views::ScrollView::ScrollBarMode::kDisabled);
  auto vertical_scroll = std::make_unique<RoundedScrollBar>(
      views::ScrollBar::Orientation::kVertical);
  vertical_scroll->SetInsets(kVerticalScrollInsets);
  scroll_view_->SetVerticalScrollBar(std::move(vertical_scroll));

  // Add margins inside the scroll contents.
  auto scroll_contents = std::make_unique<views::View>();
  scroll_contents->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetInteriorMargin(gfx::Insets(kTileSpacingInFolder))
      .SetCollapseMargins(true);

  // Create the apps grid.
  auto* items_grid_view =
      scroll_contents->AddChildView(std::make_unique<ScrollableAppsGridView>(
          a11y_announcer_, view_delegate_, this, scroll_view_,
          /*folder_controller=*/nullptr, /*keyboard_controller=*/nullptr));
  items_grid_view_ = items_grid_view;
  items_grid_view->SetMaxColumns(kMaxFolderColumns);
  items_grid_view->SetFixedTilePadding(kTileSpacingInFolder / 2,
                                       kTileSpacingInFolder / 2);
  scroll_view_->SetContents(std::move(scroll_contents));

  // In the common case, the parent view is large and the folder has a small
  // number of apps, so the scroll view's size will be limited by the apps grid
  // view's preferred size. However, if the parent view is small, the scroll
  // view will scale down, so there is enough space for the header view.
  scroll_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kPreferred));

  folder_header_view_ = contents_container_->AddChildView(
      std::make_unique<FolderHeaderView>(this, tablet_mode));
  folder_header_view_->SetProperty(views::kMarginsKey,
                                   gfx::Insets::VH(kFolderHeaderPadding, 0));

  // No margins on `contents_container_` because the scroll view needs to fully
  // extend to the parent's edges.
  contents_container_->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);
}

AppListFolderView::~AppListFolderView() {
  AppListModelProvider::Get()->RemoveObserver(this);

  // This prevents the AppsGridView's destructor from calling the now-deleted
  // AppListFolderView's methods if a drag is in progress at the time.
  items_grid_view_->set_folder_delegate(nullptr);
}

void AppListFolderView::UpdateAppListConfig(const AppListConfig* config) {
  items_grid_view_->UpdateAppListConfig(config);
}

void AppListFolderView::ConfigureForFolderItemView(
    AppListItemView* folder_item_view,
    base::OnceClosure hide_callback) {
  DCHECK(folder_item_view->is_folder());
  DCHECK(folder_item_view->item());
  DCHECK(items_grid_view_->app_list_config());

  // Clear any remaining state from the last time the folder was shown. E.g.
  // cancel any pending hide animations.
  ResetState(/*restore_folder_item_view_state=*/true);

  hide_callback_ = std::move(hide_callback);
  folder_item_view_ = folder_item_view;
  folder_item_view_observer_.Observe(folder_item_view);

  folder_item_ = static_cast<AppListFolderItem*>(folder_item_view->item());

  AppListModel* const model = AppListModelProvider::Get()->model();
  items_grid_view_->SetModel(model);
  items_grid_view_->SetItemList(folder_item_->item_list());
  folder_header_view_->SetFolderItem(folder_item_);

  model_observation_.Observe(model);

  UpdatePreferredBounds();
}

void AppListFolderView::ScheduleShowHideAnimation(bool show,
                                                  bool hide_for_reparent) {
  show_hide_metrics_tracker_ =
      GetWidget()->GetCompositor()->RequestNewThroughputTracker();
  show_hide_metrics_tracker_->Start(
      metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
        UMA_HISTOGRAM_PERCENTAGE(
            "Apps.AppListFolder.ShowHide.AnimationSmoothness", smoothness);
      })));

  folder_visibility_animations_.clear();

  shown_ = show;
  UpdateExpandedCollapsedAccessibleState();
  if (show) {
    // TODO(crbug.com/325137417): Investigate whether this line is necessary. It
    // probably isn't.
    GetViewAccessibility().SetName(
        folder_item_view_->GetViewAccessibility().GetCachedName(),
        ax::mojom::NameFrom::kAttribute);
  }

  // Animate the background corner radius, opacity and bounds.
  folder_visibility_animations_.push_back(
      std::make_unique<BackgroundAnimation>(show, this, animating_background_));

  // Animate the folder item's title's opacity.
  views::View* const folder_title = folder_item_view_->title();
  folder_title->SetPaintToLayer();
  folder_title->layer()->SetFillsBoundsOpaquely(false);
  folder_visibility_animations_.push_back(
      std::make_unique<FolderItemTitleAnimation>(show, this, folder_title));

  // Animate the bounds and opacity of items in the first page of the opened
  // folder.
  folder_visibility_animations_.push_back(std::make_unique<TopIconAnimation>(
      show, this, scroll_view_, folder_item_view_));

  // Animate the bounds and opacity of the contents container.
  folder_visibility_animations_.push_back(
      std::make_unique<ContentsContainerAnimation>(show, hide_for_reparent,
                                                   this));

  base::RepeatingClosure animation_completion_callback;
  if (!show) {
    animation_completion_callback = base::BarrierClosure(
        folder_visibility_animations_.size(),
        base::BindOnce(&AppListFolderView::OnHideAnimationDone,
                       weak_ptr_factory_.GetWeakPtr(), hide_for_reparent));
  } else {
    animation_completion_callback = base::BarrierClosure(
        folder_visibility_animations_.size(),
        base::BindOnce(&AppListFolderView::OnShowAnimationDone,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  SetVisible(true);

  background_view_->SetVisible(false);
  animating_background_->SetVisible(true);
  shadow_->GetLayer()->SetVisible(true);

  for (auto& animation : folder_visibility_animations_)
    animation->ScheduleAnimation(animation_completion_callback);
}

void AppListFolderView::AddedToWidget() {
  shadow_->ObserveColorProviderSource(GetWidget());
}

void AppListFolderView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  if (gradient_helper_)
    gradient_helper_->UpdateGradientMask();

  // `BackgroundAnimation` animates the clip rect during open/close.
  if (!IsAnimationRunning()) {
    // The folder view can change size due to app install/uninstall. Ensure the
    // rounded corners have the correct position. https://crbug.com/993282
    background_view_->layer()->SetClipRect(background_view_->GetLocalBounds());
    shadow_->SetContentBounds(background_view_->layer()->bounds());
  }
}

void AppListFolderView::ChildPreferredSizeChanged(views::View* child) {
  UpdatePreferredBounds();
  PreferredSizeChanged();
}

void AppListFolderView::OnActiveAppListModelsChanged(
    AppListModel* model,
    SearchModel* search_model) {
  // If the active model changed, close the folder view, as the backing app list
  // item is about to go away.
  if (folder_item_) {
    ResetState(/*restore_folder_item_view_state=*/false);

    folder_controller_->ShowApps(/*folder_item_view=*/nullptr,
                                 /*select_folder=*/false);
  }
}

void AppListFolderView::OnViewIsDeleting(views::View* view) {
  DCHECK_EQ(view, folder_item_view_);

  // If the original view got removed, clear any references to it, this includes
  // animations that may try to access the view to update its visibility.
  folder_visibility_animations_.clear();
  folder_item_view_observer_.Reset();
  folder_item_view_ = nullptr;
}

void AppListFolderView::OnAppListItemWillBeDeleted(AppListItem* item) {
  if (item == folder_item_) {
    ResetState(/*restore_folder_item_view_state=*/true);

    // If the folder item associated with this view is removed from the model,
    // (e.g. the last item in the folder was deleted), reset the view and signal
    // the container view to show the app list instead.
    // Pass nullptr to ShowApps() to avoid triggering animation from the deleted
    // folder.
    folder_controller_->ShowApps(/*folder_item_view=*/nullptr,
                                 /*select_folder=*/false);
  }
}

void AppListFolderView::ResetState(bool restore_folder_item_view_state) {
  DVLOG(1) << __FUNCTION__;

  if (hide_callback_)
    std::move(hide_callback_).Run();

  if (folder_item_) {
    items_grid_view_->ClearSelectedView();
    items_grid_view_->SetItemList(nullptr);
    items_grid_view_->SetModel(nullptr);
    folder_header_view_->SetFolderItem(nullptr);
    folder_item_ = nullptr;
  }

  model_observation_.Reset();

  show_hide_metrics_tracker_.reset();

  // Clear in-progress animations, as they may depend on the
  // `folder_item_view_`.
  folder_visibility_animations_.clear();

  // Transition all the states immediately to the end of folder closing
  // animation.
  background_view_->SetVisible(false);
  contents_container_->SetTransform(gfx::Transform());

  if (restore_folder_item_view_state && folder_item_view_) {
    folder_item_view_->SetIconVisible(true);
    folder_item_view_->title()->DestroyLayer();
  }

  folder_item_view_observer_.Reset();
  folder_item_view_ = nullptr;

  preferred_bounds_ = gfx::Rect();
  folder_item_icon_bounds_ = gfx::Rect();
}

void AppListFolderView::OnShowAnimationDone() {
  animating_background_->SetVisible(false);
  background_view_->SetVisible(true);

  if (animation_done_test_callback_)
    std::move(animation_done_test_callback_).Run();
}

void AppListFolderView::OnHideAnimationDone(bool hide_for_reparent) {
  animating_background_->SetVisible(false);
  shadow_->GetLayer()->SetVisible(false);

  a11y_announcer_->AnnounceFolderClosed();

  // If the folder view is hiding for folder closure, reset the
  // folder state when the animations complete. Not resetting state
  // immediately so the folder view keeps tracking folder item
  // view's liveness (so it can reset animations if the folder item
  // view gets deleted).
  // If the view is hidden for reparent, the state will be cleared
  // when the reparent drag ends - close callback still needs to be called so
  // the root apps grid knows to update its state.
  if (!hide_for_reparent) {
    ResetState(
        /*reset_folder_item_view_state=*/true);
  } else {
    if (hide_callback_)
      std::move(hide_callback_).Run();
  }

  if (animation_done_test_callback_)
    std::move(animation_done_test_callback_).Run();
}

void AppListFolderView::UpdateExpandedCollapsedAccessibleState() const {
  if (shown_) {
    GetViewAccessibility().SetIsExpanded();
  } else {
    GetViewAccessibility().SetIsCollapsed();
  }
}

void AppListFolderView::UpdatePreferredBounds() {
  if (!folder_item_view_)
    return;

  // Calculate the folder icon's bounds relative to our parent.
  gfx::RectF rect(folder_item_view_->GetIconBounds());
  ConvertRectToTarget(folder_item_view_, parent(), &rect);
  gfx::Rect icon_bounds_in_container =
      parent()->GetMirroredRect(gfx::ToEnclosingRect(rect));

  // The opened folder view's center should try to overlap with the folder
  // item's center while it must fit within the bounds of the parent.
  preferred_bounds_ = gfx::Rect(GetPreferredSize());
  preferred_bounds_ += (icon_bounds_in_container.CenterPoint() -
                        preferred_bounds_.CenterPoint());

  if (!bounding_box_.IsEmpty())
    preferred_bounds_.AdjustToFit(bounding_box_);

  // Calculate the folder icon's bounds relative to this view.
  folder_item_icon_bounds_ =
      icon_bounds_in_container - preferred_bounds_.OffsetFromOrigin();

  // Adjust folder item icon bounds for RTL (cannot use GetMirroredRect(), as
  // the current view bounds might not match the preferred bounds).
  if (base::i18n::IsRTL()) {
    folder_item_icon_bounds_.set_x(preferred_bounds_.width() -
                                   folder_item_icon_bounds_.x() -
                                   folder_item_icon_bounds_.width());
  }
}

void AppListFolderView::UpdateShadowBounds() {
  shadow_->SetContentBounds(background_view_->layer()->bounds());
}

void AppListFolderView::OnShadowLayerRecreated(ui::Layer* old_layer,
                                               ui::Layer* new_layer) {
  background_view_->RemoveLayerFromRegions(old_layer);
  background_view_->AddLayerToRegion(new_layer, views::LayerRegion::kBelow);
}

int AppListFolderView::GetYOffsetForFolder() {
  auto* const keyboard_controller = keyboard::KeyboardUIController::Get();
  if (!keyboard_controller->IsEnabled())
    return 0;

  // This view should be on top of on-screen keyboard to prevent the folder
  // title from being blocked.
  const gfx::Rect occluded_bounds =
      keyboard_controller->GetWorkspaceOccludedBoundsInScreen();
  if (!occluded_bounds.IsEmpty()) {
    gfx::Point keyboard_top_right = occluded_bounds.top_right();
    ConvertPointFromScreen(parent(), &keyboard_top_right);

    // Our final Y-Offset is determined by combining the space from the bottom
    // of the folder to the top of the keyboard, and the padding that should
    // exist between the keyboard and the folder bottom.
    // std::min() is used so that positive offsets are ignored.
    return std::min(keyboard_top_right.y() - kOnscreenKeyboardTopPadding -
                        preferred_bounds_.bottom(),
                    0);
  }

  // If no offset is calculated above, then we need none.
  return 0;
}

bool AppListFolderView::IsAnimationRunning() const {
  for (auto& animation : folder_visibility_animations_) {
    if (animation->IsAnimationRunning())
      return true;
  }
  return false;
}

void AppListFolderView::SetBoundingBox(const gfx::Rect& bounding_box) {
  bounding_box_ = bounding_box;
}

void AppListFolderView::SetAnimationDoneTestCallback(
    base::OnceClosure animation_done_callback) {
  DCHECK(!animation_done_callback || !animation_done_test_callback_);
  animation_done_test_callback_ = std::move(animation_done_callback);
}

void AppListFolderView::RecordAnimationSmoothness() {
  // RecordAnimationSmoothness is called when ContentsContainerAnimation
  // ends as well. Do not record show/hide metrics for that.
  if (show_hide_metrics_tracker_) {
    show_hide_metrics_tracker_->Stop();
    show_hide_metrics_tracker_.reset();
  }
}

void AppListFolderView::OnScrollEvent(ui::ScrollEvent* event) {
  items_grid_view_->HandleScrollFromParentView(
      gfx::Vector2d(event->x_offset(), event->y_offset()), event->type());
  event->SetHandled();
}

void AppListFolderView::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousewheel) {
    items_grid_view_->HandleScrollFromParentView(
        event->AsMouseWheelEvent()->offset(), ui::EventType::kMousewheel);
    event->SetHandled();
  }
}

bool AppListFolderView::IsDragPointOutsideOfFolder(
    const gfx::Point& drag_point) {
  // Wait for the folder bound to stabilize before starting reparent drag.
  if (IsAnimationRunning())
    return false;

  gfx::Point drag_point_in_folder = drag_point;
  views::View::ConvertPointToTarget(items_grid_view_, this,
                                    &drag_point_in_folder);
  return !GetLocalBounds().Contains(drag_point_in_folder);
}

// When user drags a folder item out of the folder boundary ink bubble, the
// folder view UI will be hidden, and switch back to top level AppsGridView.
// The dragged item will seamlessly move on the top level AppsGridView.
// In order to achieve the above, we keep the folder view and its child grid
// view visible with opacity 0, so that the drag_view_ on the hidden grid view
// will keep receiving mouse event. At the same time, we initiated a new
// drag_view_ in the top level grid view, and keep it moving with the hidden
// grid view's drag_view_, so that the dragged item can be engaged in drag and
// drop flow in the top level grid view. During the reparenting process, the
// drag_view_ in hidden grid view will dispatch the drag and drop event to
// the top level grid view, until the drag ends.
void AppListFolderView::ReparentItem(
    AppsGridView::Pointer pointer,
    AppListItemView* original_drag_view,
    const gfx::Point& drag_point_in_folder_grid) {
  // Ensures the icon updates to reflect that the icon has been removed during
  // the drag
  folder_item_view_->UpdateDraggedItem(original_drag_view->item());
  folder_item_->NotifyOfDraggedItem(original_drag_view->item());
  folder_controller_->ReparentFolderItemTransit(folder_item_);
}

void AppListFolderView::DispatchEndDragEventForReparent(
    bool events_forwarded_to_drag_drop_host,
    bool cancel_drag) {
  if (folder_item_) {
    folder_item_view_->UpdateDraggedItem(nullptr);
    folder_item_->NotifyOfDraggedItem(nullptr);
  }
  folder_controller_->ReparentDragEnded();

  // The view was not hidden in order to keeping receiving mouse events. Hide it
  // now as the reparenting ended.
  HideViewImmediately();
}

void AppListFolderView::Close() {
  CloseFolderPage();
}

void AppListFolderView::HideViewImmediately() {
  SetVisible(false);
  ResetState(/*restore_folder_item_view_state=*/true);
}

void AppListFolderView::ResetItemsGridForClose() {
  if (items_grid_view()->has_dragged_item())
    items_grid_view()->CancelDragWithNoDropAnimation();
  items_grid_view()->ClearSelectedView();
}

void AppListFolderView::CloseFolderPage() {
  DVLOG(1) << __FUNCTION__;
  // When a folder closes only show the selection highlight if there was already
  // one showing, or if the user is using ChromeVox (spoken feedback). In the
  // latter case it makes the close folder announcement more natural.
  const bool select_folder =
      items_grid_view()->has_selected_view() || IsSpokenFeedbackEnabled();
  ResetItemsGridForClose();
  folder_controller_->ShowApps(folder_item_view_, select_folder);
}

void AppListFolderView::FocusNameInput() {
  folder_header_view_->SetTextFocus();
}

void AppListFolderView::FocusFirstItem(bool silent) {
  DVLOG(1) << __FUNCTION__;
  AppListItemView* first_item_view =
      items_grid_view()->view_model()->view_at(0);
  if (silent) {
    first_item_view->SilentlyRequestFocus();
  } else {
    first_item_view->RequestFocus();
  }
}

bool AppListFolderView::IsOEMFolder() const {
  return folder_item_->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM;
}

void AppListFolderView::HandleKeyboardReparent(AppListItemView* reparented_view,
                                               ui::KeyboardCode key_code) {
  folder_controller_->ReparentFolderItemTransit(folder_item_);

  // Notify the root apps grid that folder is closing before handling keyboard
  // reparent, to match general flow during drag reparent (where close callback
  // gets called before reparenting the dragged view). This ensures that items
  // are in their ideal locations when the item gets reparented (i.e. that the
  // folder item's slot is not locked to the folder item's initial location).
  if (hide_callback_)
    std::move(hide_callback_).Run();

  root_apps_grid_view_->HandleKeyboardReparent(reparented_view,
                                               folder_item_view_, key_code);
  folder_controller_->ReparentDragEnded();
  HideViewImmediately();
}

void AppListFolderView::OnGestureEvent(ui::GestureEvent* event) {
  // Capture scroll events so they don't bubble up to the apps container, where
  // they may cause the root apps grid view to scroll, or get translated into
  // apps grid view drag.
  if (event->type() == ui::EventType::kGestureScrollBegin) {
    event->SetHandled();
  }
}

void AppListFolderView::SetItemName(AppListFolderItem* item,
                                    const std::string& name) {
  AppListModelProvider::Get()->model()->delegate()->RequestFolderRename(
      folder_item_->id(), name);
}

const AppListConfig* AppListFolderView::GetAppListConfig() const {
  return items_grid_view_->app_list_config();
}

ui::Compositor* AppListFolderView::GetCompositor() {
  return GetWidget()->GetCompositor();
}

void AppListFolderView::CancelReparentDragFromRootGrid() {
  items_grid_view_->EndDrag(/*cancel=*/true);
}

BEGIN_METADATA(AppListFolderView)
END_METADATA

}  // namespace ash
