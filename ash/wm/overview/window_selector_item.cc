// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_item.h"

#include <algorithm>
#include <vector>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_rect_view.h"
#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "ash/wm/overview/scoped_transform_overview_window.h"
#include "ash/wm/overview/start_animation_observer.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/auto_reset.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/ink_drop_mask.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {

namespace {

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
constexpr int kWindowMargin = 5;

// Cover the transformed window including the gaps between the windows with a
// transparent shield to block the input events from reaching the transformed
// window while in overview.
constexpr int kWindowSelectorMargin = kWindowMargin * 2;

// Foreground label color.
constexpr SkColor kLabelColor = SK_ColorWHITE;

// Horizontal padding for the label, on both sides.
constexpr int kHorizontalLabelPaddingDp = 12;

// Height of an item header.
constexpr int kHeaderHeightDp = 40;

// Opacity for dimmed items.
constexpr float kDimmedItemOpacity = 0.3f;

// Opacity for fading out during closing a window.
constexpr float kClosingItemOpacity = 0.8f;

// Opacity for the item header.
constexpr float kHeaderOpacity = 0.1f;

// Duration of the header and close button fade in/out when a drag is
// started/finished on a window selector item;
constexpr int kDragAnimationMs = 167;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
constexpr float kPreCloseScale = 0.02f;

// The size in dp of the window icon shown on the overview window next to the
// title.
constexpr gfx::Size kIconSize{24, 24};

// The amount we need to offset the close button so that the icon, which is
// smaller than the actual button is lined up with the right side of the window
// preview.
constexpr int kCloseButtonOffsetDp = 8;

constexpr int kCloseButtonInkDropInsetDp = 2;

// Close button color.
constexpr SkColor kCloseButtonColor = SK_ColorWHITE;

// The colors of the close button ripple.
constexpr SkColor kCloseButtonInkDropRippleColor =
    SkColorSetA(kCloseButtonColor, 0x0F);
constexpr SkColor kCloseButtonInkDropRippleHighlightColor =
    SkColorSetA(kCloseButtonColor, 0x14);

// The font delta of the overview window title.
constexpr int kLabelFontDelta = 2;

constexpr int kShadowElevation = 16;

// Values of the backdrop.
constexpr int kBackdropRoundingDp = 4;
constexpr SkColor kBackdropColor = SkColorSetARGB(0x24, 0xFF, 0xFF, 0xFF);

// The amount of translation an item animates by when it is closed by using
// swipe to close.
constexpr int kSwipeToCloseCloseTranslationDp = 96;

// Before dragging an overview window, the window will be scaled up
// |kPreDragScale| to indicate its selection.
constexpr float kDragWindowScale = 0.04f;

std::unique_ptr<views::Widget> CreateBackdropWidget(aura::Window* parent) {
  auto widget = CreateBackgroundWidget(
      /*root_window=*/nullptr, ui::LAYER_TEXTURED, kBackdropColor,
      /*border_thickness=*/0, kBackdropRoundingDp, kBackdropColor,
      /*initial_opacity=*/1.f, parent,
      /*stack_on_top=*/false);
  return widget;
}

// A Button that has a listener and listens to mouse / gesture events on the
// visible part of an overview window. Note that the drag events are only
// handled in maximized mode.
class ShieldButton : public views::Button {
 public:
  ShieldButton(views::ButtonListener* listener, const base::string16& name)
      : views::Button(listener) {
    SetAccessibleName(name);
  }
  ~ShieldButton() override = default;

  // When WindowSelectorItem (which is a ButtonListener) is destroyed, its
  // |item_widget_| is allowed to stay around to complete any animations.
  // Resetting the listener in all views that are targeted by events is
  // necessary to prevent a crash when a user clicks on the fading out widget
  // after the WindowSelectorItem has been destroyed.
  void ResetListener() { listener_ = nullptr; }

  // views::Button:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (listener()) {
      gfx::Point location(event.location());
      views::View::ConvertPointToScreen(this, &location);
      listener()->HandlePressEvent(location);
      return true;
    }
    return views::Button::OnMousePressed(event);
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (listener()) {
      gfx::Point location(event.location());
      views::View::ConvertPointToScreen(this, &location);
      listener()->HandleReleaseEvent(location);
      return;
    }
    views::Button::OnMouseReleased(event);
  }

  bool OnMouseDragged(const ui::MouseEvent& event) override {
    if (listener()) {
      gfx::Point location(event.location());
      views::View::ConvertPointToScreen(this, &location);
      listener()->HandleDragEvent(location);
      return true;
    }
    return views::Button::OnMouseDragged(event);
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (IsSlidingOutOverviewFromShelf()) {
      event->SetHandled();
      return;
    }

    if (listener()) {
      gfx::Point location(event->location());
      views::View::ConvertPointToScreen(this, &location);
      switch (event->type()) {
        case ui::ET_GESTURE_TAP_DOWN:
          listener()->HandlePressEvent(location);
          break;
        case ui::ET_GESTURE_SCROLL_UPDATE:
          listener()->HandleDragEvent(location);
          break;
        case ui::ET_SCROLL_FLING_START:
          listener()->HandleFlingStartEvent(location,
                                            event->details().velocity_x(),
                                            event->details().velocity_y());
          break;
        case ui::ET_GESTURE_SCROLL_END:
          listener()->HandleReleaseEvent(location);
          break;
        case ui::ET_GESTURE_LONG_PRESS:
          listener()->HandleLongPressEvent(location);
          break;
        case ui::ET_GESTURE_TAP:
          listener()->ActivateDraggedWindow();
          break;
        case ui::ET_GESTURE_END:
          listener()->ResetDraggedWindowGesture();
          break;
        default:
          break;
      }
      event->SetHandled();
      return;
    }
    views::Button::OnGestureEvent(event);
  }

  WindowSelectorItem* listener() {
    return static_cast<WindowSelectorItem*>(listener_);
  }

 protected:
  // views::View:
  const char* GetClassName() const override { return "ShieldButton"; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShieldButton);
};

}  // namespace

WindowSelectorItem::OverviewCloseButton::OverviewCloseButton(
    views::ButtonListener* listener)
    : views::ImageButton(listener) {
  SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);

  SetImage(views::Button::STATE_NORMAL,
           gfx::CreateVectorIcon(kOverviewWindowCloseIcon, kCloseButtonColor));
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetMinimumImageSize(gfx::Size(kHeaderHeightDp, kHeaderHeightDp));
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
}

WindowSelectorItem::OverviewCloseButton::~OverviewCloseButton() = default;

std::unique_ptr<views::InkDrop>
WindowSelectorItem::OverviewCloseButton::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      std::make_unique<views::InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(true);
  return ink_drop;
}

std::unique_ptr<views::InkDropRipple>
WindowSelectorItem::OverviewCloseButton::CreateInkDropRipple() const {
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), gfx::Insets(), GetInkDropCenterBasedOnLastEvent(),
      kCloseButtonInkDropRippleColor, /*visible_opacity=*/1.f);
}

std::unique_ptr<views::InkDropHighlight>
WindowSelectorItem::OverviewCloseButton::CreateInkDropHighlight() const {
  return std::make_unique<views::InkDropHighlight>(
      gfx::PointF(GetLocalBounds().CenterPoint()),
      std::make_unique<views::CircleLayerDelegate>(
          kCloseButtonInkDropRippleHighlightColor, GetInkDropRadius()));
}

std::unique_ptr<views::InkDropMask>
WindowSelectorItem::OverviewCloseButton::CreateInkDropMask() const {
  return std::make_unique<views::CircleInkDropMask>(
      size(), GetLocalBounds().CenterPoint(), GetInkDropRadius());
}

int WindowSelectorItem::OverviewCloseButton::GetInkDropRadius() const {
  return std::min(size().width(), size().height()) / 2 -
         kCloseButtonInkDropInsetDp;
}

// A Container View that has a ShieldButton to listen to events. The
// ShieldButton covers most of the View except for the transparent gap between
// the windows and is visually transparent. The text label does not receive
// events, however the close button is higher in Z-order than its parent and
// receives events forwarding them to the same |listener| (i.e.
// WindowSelectorItem::ButtonPressed()).
class WindowSelectorItem::CaptionContainerView : public views::View {
 public:
  enum class HeaderVisibility {
    kInvisible,
    kCloseButtonInvisibleOnly,
    kVisible,
  };

  CaptionContainerView(ButtonListener* listener,
                       views::ImageView* image_view,
                       views::Label* title_label,
                       views::Label* cannot_snap_label,
                       views::ImageButton* close_button)
      : listener_button_(new ShieldButton(listener, title_label->text())),
        image_view_(image_view),
        title_label_(title_label),
        cannot_snap_label_(cannot_snap_label),
        close_button_(close_button) {
    AddChildView(listener_button_);

    header_view_ = new views::View();
    views::BoxLayout* layout =
        header_view_->SetLayoutManager(std::make_unique<views::BoxLayout>(
            views::BoxLayout::kHorizontal, gfx::Insets(),
            kHorizontalLabelPaddingDp));
    if (image_view_)
      header_view_->AddChildView(image_view_);
    header_view_->AddChildView(title_label_);
    AddChildWithLayer(header_view_, close_button_);
    AddChildWithLayer(listener_button_, header_view_);
    layout->SetFlexForView(title_label_, 1);
  }

  ~CaptionContainerView() override {
    // If the cannot snap container was never created, delete cannot_snap_label_
    // manually.
    if (!cannot_snap_container_)
      delete cannot_snap_label_;
  }

  RoundedRectView* GetCannotSnapContainer() {
    if (!cannot_snap_container_) {
      // Use |cannot_snap_container_| to specify the padding surrounding
      // |cannot_snap_label_| and to give the label rounded corners.
      cannot_snap_container_ = new RoundedRectView(
          kSplitviewLabelRoundRectRadiusDp, kSplitviewLabelBackgroundColor);
      cannot_snap_container_->SetLayoutManager(
          std::make_unique<views::BoxLayout>(
              views::BoxLayout::kVertical,
              gfx::Insets(kSplitviewLabelVerticalInsetDp,
                          kSplitviewLabelHorizontalInsetDp)));
      cannot_snap_container_->AddChildView(cannot_snap_label_);
      cannot_snap_container_->set_can_process_events_within_subtree(false);
      AddChildWithLayer(this, cannot_snap_container_);
      cannot_snap_container_->layer()->SetOpacity(0.f);
      Layout();
    }
    return cannot_snap_container_;
  }

  ShieldButton* listener_button() { return listener_button_; }

  gfx::Rect backdrop_bounds() const { return backdrop_bounds_; }

  void SetHeaderVisibility(HeaderVisibility visibility) {
    DCHECK(close_button_->layer());
    DCHECK(header_view_->layer());

    // Make the close button invisible if the rest of the header is to be shown.
    // If the rest of the header is to be hidden, make the close button visible
    // as |header_view_|'s opacity will be 0.f, hiding the close button. Modify
    // |close_button_|'s opacity instead of visibilty so the flex from its
    // sibling views do not mess up its layout.
    close_button_->layer()->SetOpacity(
        visibility == HeaderVisibility::kCloseButtonInvisibleOnly ? 0.f : 1.f);
    const bool visible = visibility != HeaderVisibility::kInvisible;
    AnimateLayerOpacity(header_view_->layer(), visible);
  }

  void SetCannotSnapLabelVisibility(bool visible) {
    if (!cannot_snap_container_ && !visible)
      return;

    DoSplitviewOpacityAnimation(
        GetCannotSnapContainer()->layer(),
        visible ? SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_IN
                : SPLITVIEW_ANIMATION_SELECTOR_ITEM_FADE_OUT);
  }

  views::View* header_view() { return header_view_; }

 protected:
  // views::View:
  void Layout() override {
    // |listener_button_| serves as a shield to prevent input events from
    // reaching the transformed window in overview.
    gfx::Rect bounds(GetLocalBounds());
    bounds.Inset(kWindowSelectorMargin, kWindowSelectorMargin);
    listener_button_->SetBoundsRect(bounds);

    // Position the cannot snap label.
    gfx::Size label_size = cannot_snap_label_->CalculatePreferredSize();
    label_size.set_width(
        std::min(label_size.width() + 2 * kSplitviewLabelHorizontalInsetDp,
                 bounds.width() - 2 * kSplitviewLabelHorizontalInsetDp));
    label_size.set_height(
        std::max(label_size.height(), kSplitviewLabelPreferredHeightDp));

    const int visible_height = close_button_->GetPreferredSize().height();
    backdrop_bounds_ = bounds;
    backdrop_bounds_.Inset(0, visible_height, 0, 0);

    if (cannot_snap_container_) {
      // Position the cannot snap label in the middle of the item, minus the
      // title.
      gfx::Rect cannot_snap_bounds = GetLocalBounds();
      cannot_snap_bounds.Inset(0, visible_height, 0, 0);
      cannot_snap_bounds.ClampToCenteredSize(label_size);
      cannot_snap_container_->SetBoundsRect(cannot_snap_bounds);
    }

    // Position the header at the top. The right side of the header should be
    // positioned so that the rightmost of the close icon matches the right side
    // of the window preview.
    gfx::Rect header_bounds = GetLocalBounds();
    header_bounds.Inset(0, 0, kCloseButtonOffsetDp, 0);
    header_bounds.set_height(visible_height);
    header_view_->SetBoundsRect(header_bounds);
  }

  const char* GetClassName() const override { return "CaptionContainerView"; }

 private:
  // Animates |layer| from 0 -> 1 opacity if |visible| and 1 -> 0 opacity
  // otherwise. The tween type differs for |visible| and if |visible| is true
  // there is a slight delay before the animation begins. Does not animate if
  // opacity matches |visible|.
  void AnimateLayerOpacity(ui::Layer* layer, bool visible) {
    float target_opacity = visible ? 1.f : 0.f;
    if (layer->GetTargetOpacity() == target_opacity)
      return;

    layer->SetOpacity(1.f - target_opacity);
    {
      ui::LayerAnimator* animator = layer->GetAnimator();
      ui::ScopedLayerAnimationSettings settings(animator);
      settings.SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      if (visible) {
        animator->SchedulePauseForProperties(
            base::TimeDelta::FromMilliseconds(kDragAnimationMs),
            ui::LayerAnimationElement::OPACITY);
      }
      settings.SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kDragAnimationMs));
      settings.SetTweenType(visible ? gfx::Tween::LINEAR_OUT_SLOW_IN
                                    : gfx::Tween::FAST_OUT_LINEAR_IN);
      layer->SetOpacity(target_opacity);
    }
  }

  // Helper function to add a child view to a parent view and make it paint to
  // layer.
  static void AddChildWithLayer(views::View* parent, views::View* child) {
    child->SetPaintToLayer();
    child->layer()->SetFillsBoundsOpaquely(false);
    parent->AddChildView(child);
  };

  ShieldButton* listener_button_;
  views::ImageView* image_view_;
  views::Label* title_label_;
  views::Label* cannot_snap_label_;
  RoundedRectView* cannot_snap_container_ = nullptr;
  views::ImageButton* close_button_;
  // View which contains the icon, title and close button.
  views::View* header_view_ = nullptr;
  gfx::Rect backdrop_bounds_;

  DISALLOW_COPY_AND_ASSIGN(CaptionContainerView);
};

WindowSelectorItem::WindowSelectorItem(aura::Window* window,
                                       WindowSelector* window_selector,
                                       WindowGrid* window_grid)
    : root_window_(window->GetRootWindow()),
      transform_window_(this, window),
      close_button_(new OverviewCloseButton(this)),
      window_selector_(window_selector),
      window_grid_(window_grid) {
  CreateWindowLabel(window->GetTitle());
  GetWindow()->AddObserver(this);
  if (GetWindowDimensionsType() !=
      ScopedTransformOverviewWindow::GridWindowFillMode::kNormal) {
    backdrop_widget_ = CreateBackdropWidget(window->parent());
  }
  GetWindow()->SetProperty(ash::kIsShowingInOverviewKey, true);
}

WindowSelectorItem::~WindowSelectorItem() {
  GetWindow()->RemoveObserver(this);
  GetWindow()->ClearProperty(ash::kIsShowingInOverviewKey);
}

aura::Window* WindowSelectorItem::GetWindow() {
  return transform_window_.window();
}

aura::Window* WindowSelectorItem::GetWindowForStacking() {
  // If the window is minimized, stack |widget_window| above the minimized
  // window, otherwise the minimized window will cover |widget_window|. The
  // minimized is created with the same parent as the original window, just
  // like |item_widget_|, so there is no need to reparent.
  return transform_window_.minimized_widget()
             ? transform_window_.minimized_widget()->GetNativeWindow()
             : GetWindow();
}

bool WindowSelectorItem::Contains(const aura::Window* target) const {
  return transform_window_.Contains(target);
}

void WindowSelectorItem::RestoreWindow(bool reset_transform) {
  caption_container_view_->listener_button()->ResetListener();
  close_button_->ResetListener();
  transform_window_.RestoreWindow(
      reset_transform,
      window_selector_->enter_exit_overview_type() ==
          WindowSelector::EnterExitOverviewType::kWindowsMinimized);
}

void WindowSelectorItem::EnsureVisible() {
  transform_window_.EnsureVisible();
}

void WindowSelectorItem::Shutdown() {
  if (transform_window_.GetTopInset()) {
    // Activating a window (even when it is the window that was active before
    // overview) results in stacking it at the top. Maintain the label window
    // stacking position above the item to make the header transformation more
    // gradual upon exiting the overview mode.
    aura::Window* widget_window = item_widget_->GetNativeWindow();

    // |widget_window| was originally created in the same container as the
    // |transform_window_| but when closing overview the |transform_window_|
    // could have been reparented if a drag was active. Only change stacking
    // if the windows still belong to the same container.
    if (widget_window->parent() == transform_window_.window()->parent()) {
      widget_window->parent()->StackChildAbove(widget_window,
                                               transform_window_.window());
    }
  }

  // On swiping from the shelf, the caller handles the animation via calls to
  // UpdateYAndOpacity, so do not additional fade out or slide animation to the
  // window.
  if (window_selector_->enter_exit_overview_type() ==
      WindowSelector::EnterExitOverviewType::kSwipeFromShelf) {
    return;
  }

  // Fade out the item widget. This animation continues past the lifetime
  // of |this|.
  const bool slide = window_selector_->enter_exit_overview_type() ==
                     WindowSelector::EnterExitOverviewType::kWindowsMinimized;
  FadeOutWidgetAndMaybeSlideOnExit(
      std::move(item_widget_),
      slide ? OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER
            : OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT,
      slide);
}

void WindowSelectorItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  RestackItemWidget();
  UpdateHeaderLayout(HeaderFadeInMode::kEnter, OVERVIEW_ANIMATION_NONE);
}

void WindowSelectorItem::SlideWindowIn() {
  // |transform_window_|'s |minimized_widget| is non null because this only gets
  // called if we see the home launcher on enter (all windows are minimized).
  DCHECK(item_widget_);
  DCHECK(transform_window_.minimized_widget());

  FadeInWidgetAndMaybeSlideOnEnter(item_widget_.get(),
                                   OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
                                   /*slide=*/true);
  FadeInWidgetAndMaybeSlideOnEnter(transform_window_.minimized_widget(),
                                   OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER,
                                   /*slide=*/true);
}

void WindowSelectorItem::UpdateYPositionAndOpacity(
    int new_grid_y,
    float opacity,
    WindowSelector::UpdateAnimationSettingsCallback callback) {
  // Animate the window selector widget and the window itself.
  // TODO(sammiequon): Investigate if we can combine with
  // FadeInWidgetAndMaybeSlideOnEnter and animate the transient children too.
  // Also when animating we should remove shadow and rounded corners.
  std::vector<ui::Layer*> animation_layers = {
      GetWindowForStacking()->layer(),
      item_widget_->GetNativeWindow()->layer()};
  for (auto* layer : animation_layers) {
    layer->GetAnimator()->StopAnimating();
    std::unique_ptr<ui::ScopedLayerAnimationSettings> settings;
    if (!callback.is_null()) {
      settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          layer->GetAnimator());
      callback.Run(settings.get(), /*observe=*/false);
    }
    layer->SetOpacity(opacity);

    // Alter the y-translation. Offset by the window location relative to the
    // grid.
    const int offset = target_bounds_.y() + kHeaderHeightDp + kWindowMargin;
    gfx::Transform transform = layer->transform();
    transform.matrix().setFloat(1, 3, static_cast<float>(offset + new_grid_y));
    layer->SetTransform(transform);
  }
}

float WindowSelectorItem::GetItemScale(const gfx::Size& size) {
  gfx::Size inset_size(size.width(), size.height() - 2 * kWindowMargin);
  return ScopedTransformOverviewWindow::GetItemScale(
      GetTargetBoundsInScreen().size(), inset_size,
      transform_window_.GetTopInset(),
      close_button_->GetPreferredSize().height());
}

gfx::Rect WindowSelectorItem::GetTargetBoundsInScreen() const {
  return ::ash::GetTargetBoundsInScreen(transform_window_.GetOverviewWindow());
}

gfx::Rect WindowSelectorItem::GetTransformedBounds() const {
  return transform_window_.GetTransformedBounds();
}

void WindowSelectorItem::SetBounds(const gfx::Rect& target_bounds,
                                   OverviewAnimationType animation_type) {
  if (in_bounds_update_)
    return;

  // Do not animate if the resulting bounds does not change. The original
  // window may change bounds so we still need to call SetItemBounds to update
  // the window transform.
  OverviewAnimationType new_animation_type = animation_type;
  if (target_bounds == target_bounds_)
    new_animation_type = OVERVIEW_ANIMATION_NONE;

  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  // If |target_bounds_| is empty, this is the first update. Let
  // UpdateHeaderLayout know, as we do not want |item_widget_| to be animated
  // with the window.
  HeaderFadeInMode mode = target_bounds_.IsEmpty()
                              ? HeaderFadeInMode::kFirstUpdate
                              : HeaderFadeInMode::kUpdate;
  target_bounds_ = target_bounds;

  gfx::Rect inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);

  // Do not animate if entering when the window is minimized, as it will be
  // faded in. We still want to animate if the position is changed after
  // entering.
  if (wm::GetWindowState(GetWindow())->IsMinimized() &&
      mode == HeaderFadeInMode::kFirstUpdate) {
    new_animation_type = OVERVIEW_ANIMATION_NONE;
  }

  SetItemBounds(inset_bounds, new_animation_type);

  // SetItemBounds is called before UpdateHeaderLayout so the header can
  // properly use the updated windows bounds.
  UpdateHeaderLayout(mode, new_animation_type);

  // Shadow is normally set after an animation is finished. In the case of no
  // animations, manually set the shadow. Shadow relies on both the window
  // transform and |item_widget_|'s new bounds so set it after SetItemBounds
  // and UpdateHeaderLayout. Do not apply the shadow for drop target.
  if (new_animation_type == OVERVIEW_ANIMATION_NONE) {
    SetShadowBounds(
        window_grid_->IsDropTargetWindow(GetWindow())
            ? base::nullopt
            : base::make_optional(transform_window_.GetTransformedBounds()));
  }

  UpdateBackdropBounds();
}

void WindowSelectorItem::SendAccessibleSelectionEvent() {
  caption_container_view_->listener_button()->NotifyAccessibilityEvent(
      ax::mojom::Event::kSelection, true);
}

void WindowSelectorItem::AnimateAndCloseWindow(bool up) {
  base::RecordAction(base::UserMetricsAction("WindowSelector_SwipeToClose"));

  animating_to_close_ = true;
  window_selector_->PositionWindows(/*animate=*/true);
  caption_container_view_->listener_button()->ResetListener();
  close_button_->ResetListener();

  int translation_y = kSwipeToCloseCloseTranslationDp * (up ? -1 : 1);
  gfx::Transform transform;
  transform.Translate(gfx::Vector2d(0, translation_y));

  auto animate_window = [this](aura::Window* window,
                               const gfx::Transform& transform, bool observe) {
    ScopedOverviewAnimationSettings settings(
        OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM, window);
    gfx::Transform original_transform = window->transform();
    original_transform.ConcatTransform(transform);
    window->SetTransform(original_transform);
    if (observe)
      settings.AddObserver(this);
  };

  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM);
  animate_window(item_widget_->GetNativeWindow(), transform, false);
  animate_window(GetWindowForStacking(), transform, true);
}

void WindowSelectorItem::CloseWindow() {
  gfx::Rect inset_bounds(target_bounds_);
  inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                     target_bounds_.height() * kPreCloseScale);
  // Scale down both the window and label.
  SetBounds(inset_bounds, OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM);
  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(0.0, OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM);
  transform_window_.Close();
}

void WindowSelectorItem::OnMinimizedStateChanged() {
  transform_window_.UpdateMirrorWindowForMinimizedState();
}

void WindowSelectorItem::UpdateCannotSnapWarningVisibility() {
  // Windows which can snap will never show this warning. Or if the window is
  // the drop target window, also do not show this warning.
  if (Shell::Get()->split_view_controller()->CanSnap(GetWindow()) ||
      window_grid_->IsDropTargetWindow(GetWindow())) {
    caption_container_view_->SetCannotSnapLabelVisibility(false);
    return;
  }

  const SplitViewController::State state =
      Shell::Get()->split_view_controller()->state();
  const bool visible = state == SplitViewController::LEFT_SNAPPED ||
                       state == SplitViewController::RIGHT_SNAPPED;
  caption_container_view_->SetCannotSnapLabelVisibility(visible);
}

void WindowSelectorItem::OnSelectorItemDragStarted(WindowSelectorItem* item) {
  caption_container_view_->SetHeaderVisibility(
      item == this
          ? CaptionContainerView::HeaderVisibility::kInvisible
          : CaptionContainerView::HeaderVisibility::kCloseButtonInvisibleOnly);
}

void WindowSelectorItem::OnSelectorItemDragEnded() {
  caption_container_view_->SetHeaderVisibility(
      CaptionContainerView::HeaderVisibility::kVisible);
}

ScopedTransformOverviewWindow::GridWindowFillMode
WindowSelectorItem::GetWindowDimensionsType() const {
  return transform_window_.type();
}

void WindowSelectorItem::UpdateWindowDimensionsType() {
  transform_window_.UpdateWindowDimensionsType();
  if (GetWindowDimensionsType() ==
      ScopedTransformOverviewWindow::GridWindowFillMode::kNormal) {
    // Delete the backdrop widget, if it exists for normal windows.
    if (backdrop_widget_)
      backdrop_widget_.reset();
  } else {
    // Create the backdrop widget if needed.
    if (!backdrop_widget_) {
      backdrop_widget_ =
          CreateBackdropWidget(transform_window_.window()->parent());
    }
  }
}

void WindowSelectorItem::EnableBackdropIfNeeded() {
  if (GetWindowDimensionsType() ==
      ScopedTransformOverviewWindow::GridWindowFillMode::kNormal) {
    DisableBackdrop();
    return;
  }

  UpdateBackdropBounds();
}

void WindowSelectorItem::DisableBackdrop() {
  if (backdrop_widget_)
    backdrop_widget_->Hide();
}

void WindowSelectorItem::UpdateBackdropBounds() {
  if (!backdrop_widget_)
    return;

  gfx::Rect backdrop_bounds = caption_container_view_->backdrop_bounds();
  ::wm::ConvertRectToScreen(item_widget_->GetNativeWindow(), &backdrop_bounds);
  backdrop_widget_->SetBounds(backdrop_bounds);
  backdrop_widget_->Show();
}

gfx::Rect WindowSelectorItem::GetBoundsOfSelectedItem() {
  gfx::Rect original_bounds = target_bounds();
  ScaleUpSelectedItem(OVERVIEW_ANIMATION_NONE);
  gfx::Rect selected_bounds = transform_window_.GetTransformedBounds();
  SetBounds(original_bounds, OVERVIEW_ANIMATION_NONE);
  return selected_bounds;
}

void WindowSelectorItem::ScaleUpSelectedItem(
    OverviewAnimationType animation_type) {
  gfx::Rect scaled_bounds(target_bounds());
  scaled_bounds.Inset(-scaled_bounds.width() * kDragWindowScale,
                      -scaled_bounds.height() * kDragWindowScale);
  SetBounds(scaled_bounds, animation_type);
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::RestackItemWidget() {
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  widget_window->parent()->StackChildAbove(widget_window,
                                           GetWindowForStacking());
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (IsSlidingOutOverviewFromShelf())
    return;

  if (sender == close_button_) {
    base::RecordAction(
        base::UserMetricsAction("WindowSelector_OverviewCloseButton"));
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled()) {
      base::RecordAction(
          base::UserMetricsAction("Tablet_WindowCloseFromOverviewButton"));
    }
    CloseWindow();
    return;
  }

  CHECK(sender == caption_container_view_->listener_button());

  // For other cases, the event is handled in OverviewWindowDragController.
  if (!SplitViewController::ShouldAllowSplitView())
    window_selector_->SelectWindow(this);
}

void WindowSelectorItem::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (reason == ui::PropertyChangeReason::NOT_FROM_ANIMATION)
    transform_window_.ResizeMinimizedWidgetIfNeeded();
}

void WindowSelectorItem::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  transform_window_.OnWindowDestroyed();
}

void WindowSelectorItem::OnWindowTitleChanged(aura::Window* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  label_view_->SetText(window->GetTitle());
  UpdateAccessibilityName();
}

void WindowSelectorItem::OnImplicitAnimationsCompleted() {
  transform_window_.Close();
}

void WindowSelectorItem::HandlePressEvent(
    const gfx::Point& location_in_screen) {
  // We allow switching finger while dragging, but do not allow dragging two or more items.
  if (window_selector_->window_drag_controller() &&
      window_selector_->window_drag_controller()->item()) {
    return;
  }

  StartDrag();
  window_selector_->InitiateDrag(this, location_in_screen);
}

void WindowSelectorItem::HandleReleaseEvent(
    const gfx::Point& location_in_screen) {
  if (!IsDragItem())
    return;
  window_grid_->SetSelectionWidgetVisibility(true);
  window_selector_->CompleteDrag(this, location_in_screen);
}

void WindowSelectorItem::HandleDragEvent(const gfx::Point& location_in_screen) {
  if (!IsDragItem())
    return;

  window_selector_->Drag(this, location_in_screen);
}

void WindowSelectorItem::HandleLongPressEvent(
    const gfx::Point& location_in_screen) {
  if (!SplitViewController::ShouldAllowSplitView())
    return;

  window_selector_->StartSplitViewDragMode(location_in_screen);
}

void WindowSelectorItem::HandleFlingStartEvent(
    const gfx::Point& location_in_screen,
    float velocity_x,
    float velocity_y) {
  window_selector_->Fling(this, location_in_screen, velocity_x, velocity_y);
}

void WindowSelectorItem::ActivateDraggedWindow() {
  if (!IsDragItem())
    return;

  window_selector_->ActivateDraggedWindow();
}

void WindowSelectorItem::ResetDraggedWindowGesture() {
  if (!IsDragItem())
    return;

  OnSelectorItemDragEnded();
  window_selector_->ResetDraggedWindowGesture();
}

bool WindowSelectorItem::IsDragItem() {
  return window_selector_->window_drag_controller() &&
         window_selector_->window_drag_controller()->item() == this;
}

void WindowSelectorItem::OnDragAnimationCompleted() {
  // This is function is called whenever the grid repositions its windows, but
  // we only need to restack the windows if an item was being dragged around
  // and then released.
  if (!should_restack_on_animation_end_)
    return;

  should_restack_on_animation_end_ = false;

  // First stack this item's window below the snapped window if split view mode
  // is active.
  aura::Window* dragged_window = GetWindowForStacking();
  aura::Window* dragged_widget_window = item_widget_->GetNativeWindow();
  aura::Window* parent_window = dragged_widget_window->parent();
  if (Shell::Get()->IsSplitViewModeActive()) {
    aura::Window* snapped_window =
        Shell::Get()->split_view_controller()->GetDefaultSnappedWindow();
    if (snapped_window->parent() == parent_window &&
        dragged_window->parent() == parent_window) {
      parent_window->StackChildBelow(dragged_widget_window, snapped_window);
      parent_window->StackChildBelow(dragged_window, dragged_widget_window);
    }
  }

  // Then find the window which was stacked right above this selector item's
  // window before dragging and stack this selector item's window below it.
  const std::vector<std::unique_ptr<WindowSelectorItem>>& selector_items =
      window_grid_->window_list();
  aura::Window* stacking_target = nullptr;
  for (size_t index = 0; index < selector_items.size(); index++) {
    if (index > 0) {
      aura::Window* window = selector_items[index - 1].get()->GetWindow();
      if (window->parent() == parent_window &&
          dragged_window->parent() == parent_window) {
        stacking_target = window;
      }
    }
    if (selector_items[index].get() == this && stacking_target) {
      parent_window->StackChildBelow(dragged_widget_window, stacking_target);
      parent_window->StackChildBelow(dragged_window, dragged_widget_window);
      break;
    }
  }
}

void WindowSelectorItem::SetShadowBounds(
    base::Optional<gfx::Rect> bounds_in_screen) {
  // Shadow is normally turned off during animations and reapplied when they
  // are finished. On destruction, |shadow_| is cleaned up before
  // |transform_window_|, which may call this function, so early exit if
  // |shadow_| is nullptr.
  if (!shadow_)
    return;

  if (bounds_in_screen == base::nullopt) {
    shadow_->layer()->SetVisible(false);
    return;
  }

  shadow_->layer()->SetVisible(true);
  gfx::Rect bounds_in_item =
      gfx::Rect(item_widget_->GetNativeWindow()->GetTargetBounds().size());
  bounds_in_item.Inset(kWindowSelectorMargin, kWindowSelectorMargin);
  bounds_in_item.Inset(0, close_button_->GetPreferredSize().height(), 0, 0);
  bounds_in_item.ClampToCenteredSize(bounds_in_screen.value().size());

  shadow_->SetContentBounds(bounds_in_item);
}

void WindowSelectorItem::UpdateMaskAndShadow(bool show) {
  transform_window_.UpdateMask(show);

  // Do not apply the shadow for the drop target in overview.
  if (!show || window_grid_->IsDropTargetWindow(GetWindow())) {
    SetShadowBounds(base::nullopt);
    DisableBackdrop();
    return;
  }

  SetShadowBounds(transform_window_.GetTransformedBounds());
  EnableBackdropIfNeeded();
}

void WindowSelectorItem::SetOpacity(float opacity) {
  item_widget_->SetOpacity(opacity);
  transform_window_.SetOpacity(opacity);
}

float WindowSelectorItem::GetOpacity() {
  return item_widget_->GetNativeWindow()->layer()->opacity();
}

OverviewAnimationType WindowSelectorItem::GetExitOverviewAnimationType() {
  return should_animate_when_exiting_
             ? OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS_ON_EXIT
             : OVERVIEW_ANIMATION_NONE;
}

OverviewAnimationType WindowSelectorItem::GetExitTransformAnimationType() {
  return should_animate_when_exiting_ ? OVERVIEW_ANIMATION_RESTORE_WINDOW
                                      : OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO;
}

float WindowSelectorItem::GetCloseButtonVisibilityForTesting() const {
  return close_button_->layer()->opacity();
}

float WindowSelectorItem::GetTitlebarOpacityForTesting() const {
  return caption_container_view_->header_view()->layer()->opacity();
}

gfx::Rect WindowSelectorItem::GetShadowBoundsForTesting() {
  if (!shadow_ || !shadow_->layer()->visible())
    return gfx::Rect();

  return shadow_->content_bounds();
}

void WindowSelectorItem::SetItemBounds(const gfx::Rect& target_bounds,
                                       OverviewAnimationType animation_type) {
  aura::Window* window = GetWindow();
  DCHECK(root_window_ == window->GetRootWindow());
  gfx::Rect screen_rect = GetTargetBoundsInScreen();

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::Size screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::Size(1, 1));
  screen_rect.set_size(screen_size);

  const int top_view_inset = transform_window_.GetTopInset();
  const int title_height = close_button_->GetPreferredSize().height();
  gfx::Rect selector_item_bounds =
      transform_window_.ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, title_height);
  // Do not set transform for drop target, set bounds instead.
  if (window_grid_->IsDropTargetWindow(window)) {
    window->layer()->SetBounds(selector_item_bounds);
    transform_window_.GetOverviewWindow()->SetTransform(gfx::Transform());
    return;
  }

  gfx::Transform transform = ScopedTransformOverviewWindow::GetTransformForRect(
      screen_rect, selector_item_bounds);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  SetTransform(transform_window_.GetOverviewWindow(), transform);
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  views::Widget::InitParams params_label;
  params_label.type = views::Widget::InitParams::TYPE_POPUP;
  params_label.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_label.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params_label.visible_on_all_workspaces = true;
  params_label.layer_type = ui::LAYER_NOT_DRAWN;
  params_label.name = "OverviewModeLabel";
  params_label.activatable =
      views::Widget::InitParams::Activatable::ACTIVATABLE_DEFAULT;
  params_label.accept_events = true;
  item_widget_ = std::make_unique<views::Widget>();
  params_label.parent = transform_window_.window()->parent();
  item_widget_->set_focus_on_creation(false);
  item_widget_->Init(params_label);
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  // Stack the widget above the transform window so that it can block events.
  widget_window->parent()->StackChildAbove(widget_window,
                                           transform_window_.window());

  // Create an image view the header icon. Tries to use the app icon, as it is
  // higher resolution. If it does not exist, use the window icon. If neither
  // exist, display nothing.
  views::ImageView* image_view = nullptr;
  gfx::ImageSkia* icon = GetWindow()->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty())
    icon = GetWindow()->GetProperty(aura::client::kWindowIconKey);
  if (icon && !icon->size().IsEmpty()) {
    image_view = new views::ImageView();
    image_view->SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
        *icon, skia::ImageOperations::RESIZE_BEST, kIconSize));
    image_view->SetSize(kIconSize);
  }

  label_view_ = new views::Label(title);
  label_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_view_->SetAutoColorReadabilityEnabled(false);
  label_view_->SetEnabledColor(kLabelColor);
  label_view_->SetSubpixelRenderingEnabled(false);
  label_view_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));

  shadow_ = std::make_unique<ui::Shadow>();
  shadow_->Init(kShadowElevation);
  item_widget_->GetLayer()->Add(shadow_->layer());

  cannot_snap_label_view_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_SPLIT_VIEW_CANNOT_SNAP));
  cannot_snap_label_view_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  cannot_snap_label_view_->SetAutoColorReadabilityEnabled(false);
  cannot_snap_label_view_->SetEnabledColor(kSplitviewLabelEnabledColor);
  cannot_snap_label_view_->SetBackgroundColor(kSplitviewLabelBackgroundColor);

  caption_container_view_ = new CaptionContainerView(
      this, image_view, label_view_, cannot_snap_label_view_, close_button_);
  UpdateCannotSnapWarningVisibility();

  item_widget_->SetContentsView(caption_container_view_);
  item_widget_->Show();
  item_widget_->SetOpacity(0);
  item_widget_->GetLayer()->SetMasksToBounds(false);
  FadeInWidgetAndMaybeSlideOnEnter(
      item_widget_.get(), OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
      /*slide=*/false);
}

void WindowSelectorItem::UpdateHeaderLayout(
    HeaderFadeInMode mode,
    OverviewAnimationType animation_type) {
  gfx::Rect transformed_window_bounds =
      transform_window_.window_selector_bounds().value_or(
          transform_window_.GetTransformedBounds());
  ::wm::ConvertRectFromScreen(root_window_, &transformed_window_bounds);

  gfx::Rect label_rect(close_button_->GetPreferredSize());
  label_rect.set_width(transformed_window_bounds.width());
  // For tabbed windows the initial bounds of the caption are set such that it
  // appears to be "growing" up from the window content area.
  label_rect.set_y(
      (mode != HeaderFadeInMode::kEnter || transform_window_.GetTopInset())
          ? -label_rect.height()
          : 0);

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  // For the first update, place the widget at its destination.
  ScopedOverviewAnimationSettings animation_settings(
      mode == HeaderFadeInMode::kFirstUpdate ? OVERVIEW_ANIMATION_NONE
                                             : animation_type,
      widget_window);

  // Create a start animation observer if this is an enter overview layout
  // animation.
  if (animation_type == OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS_ON_ENTER) {
    auto start_observer = std::make_unique<StartAnimationObserver>();
    animation_settings.AddObserver(start_observer.get());
    Shell::Get()->window_selector_controller()->AddStartAnimationObserver(
        std::move(start_observer));
  }

  // |widget_window| covers both the transformed window and the header
  // as well as the gap between the windows to prevent events from reaching
  // the window including its sizing borders.
  if (mode != HeaderFadeInMode::kEnter) {
    label_rect.set_height(close_button_->GetPreferredSize().height() +
                          transformed_window_bounds.height());
  }
  label_rect.Inset(-kWindowSelectorMargin, -kWindowSelectorMargin);
  widget_window->SetBounds(label_rect);
  gfx::Transform label_transform;
  label_transform.Translate(transformed_window_bounds.x(),
                            transformed_window_bounds.y());
  widget_window->SetTransform(label_transform);
}

void WindowSelectorItem::AnimateOpacity(float opacity,
                                        OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  const float header_opacity = selected_ ? 0.f : kHeaderOpacity * opacity;
  aura::Window* widget_window = item_widget_->GetNativeWindow();
  ScopedOverviewAnimationSettings animation_settings_label(animation_type,
                                                           widget_window);
  widget_window->layer()->SetOpacity(header_opacity);
}

void WindowSelectorItem::UpdateAccessibilityName() {
  caption_container_view_->listener_button()->SetAccessibleName(
      GetWindow()->GetTitle());
}

aura::Window* WindowSelectorItem::GetOverviewWindowForMinimizedStateForTest() {
  return transform_window_.GetOverviewWindowForMinimizedState();
}

void WindowSelectorItem::StartDrag() {
  window_grid_->SetSelectionWidgetVisibility(false);

  // |transform_window_| handles hiding shadow and rounded edges mask while
  // animating, and applies them after animation is complete. Prevent the
  // shadow and rounded edges mask from showing up after dragging in the case
  // the window is pressed while still animating.
  transform_window_.CancelAnimationsListener();

  aura::Window* widget_window = item_widget_->GetNativeWindow();
  aura::Window* window = GetWindowForStacking();
  if (widget_window && widget_window->parent() == window->parent()) {
    // TODO(xdai): This might not work if there is an always on top window.
    // See crbug.com/733760.
    widget_window->parent()->StackChildAtTop(widget_window);
    widget_window->parent()->StackChildBelow(window, widget_window);
  }
}

}  // namespace ash
