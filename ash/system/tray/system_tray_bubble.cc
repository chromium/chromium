// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_bubble.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Normally a detailed view is the same size as the default view. However,
// when showing a detailed view directly (e.g. clicking on a notification),
// we may not know the height of the default view, or the default view may
// be too short, so we use this as a default and minimum height for any
// detailed view.
int GetDetailedBubbleMaxHeight() {
  return kTrayPopupItemMinHeight * 5;
}

// Duration of swipe animation used when transitioning from a default to
// detailed view or vice versa.
const int kSwipeDelayMS = 150;

// Extra bottom padding when showing the SYSTEM_TRAY_TYPE_DEFAULT view.
const int kDefaultViewBottomPadding = 4;

// Implicit animation observer that deletes itself and the layer at the end of
// the animation.
class AnimationObserverDeleteLayer : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverDeleteLayer(ui::Layer* layer) : layer_(layer) {}

  ~AnimationObserverDeleteLayer() override = default;

  void OnImplicitAnimationsCompleted() override {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
  }

 private:
  std::unique_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserverDeleteLayer);
};

}  // namespace

// SystemTrayBubble

SystemTrayBubble::SystemTrayBubble(SystemTray* tray)
    : tray_(tray), autoclose_delay_(0) {}

SystemTrayBubble::~SystemTrayBubble() {
  // Reset the host pointer in bubble_view_ in case its destruction is deferred.
  if (bubble_view_)
    bubble_view_->ResetDelegate();
}

void SystemTrayBubble::UpdateView(
    const std::vector<ash::SystemTrayItem*>& items,
    SystemTrayView::SystemTrayType system_tray_type) {
  std::unique_ptr<ui::Layer> scoped_layer;
  if (system_tray_type != system_tray_view_->system_tray_type()) {
    base::TimeDelta swipe_duration =
        base::TimeDelta::FromMilliseconds(kSwipeDelayMS);
    scoped_layer = bubble_view_->RecreateLayer();
    // Keep the reference to layer as we need it after releasing it.
    ui::Layer* layer = scoped_layer.get();
    DCHECK(layer);
    layer->SuppressPaint();

    // When transitioning from detailed view to default view, animate the
    // existing view (slide out towards the right).
    if (system_tray_type == SystemTrayView::SYSTEM_TRAY_TYPE_DEFAULT) {
      ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
      settings.AddObserver(
          new AnimationObserverDeleteLayer(scoped_layer.release()));
      settings.SetTransitionDuration(swipe_duration);
      settings.SetTweenType(gfx::Tween::EASE_OUT);
      gfx::Transform transform;
      transform.Translate(layer->bounds().width(), 0.0);
      layer->SetTransform(transform);
    }

    {
      // Add a shadow layer to make the old layer darker as the animation
      // progresses.
      ui::Layer* shadow = new ui::Layer(ui::LAYER_SOLID_COLOR);
      shadow->SetColor(SK_ColorBLACK);
      shadow->SetOpacity(0.01f);
      shadow->SetBounds(layer->bounds());
      layer->Add(shadow);
      layer->StackAtTop(shadow);
      {
        // Animate the darkening effect a little longer than the swipe-in. This
        // is to make sure the darkening animation does not end up finishing
        // early, because the dark layer goes away at the end of the animation,
        // and there is a brief moment when the old view is still visible, but
        // it does not have the shadow layer on top.
        ui::ScopedLayerAnimationSettings settings(shadow->GetAnimator());
        settings.AddObserver(new AnimationObserverDeleteLayer(shadow));
        settings.SetTransitionDuration(swipe_duration +
                                       base::TimeDelta::FromMilliseconds(150));
        settings.SetTweenType(gfx::Tween::LINEAR);
        shadow->SetOpacity(0.15f);
      }
    }
  }

  system_tray_view_->DestroyItemViews();
  system_tray_view_->RemoveAllChildViews(true);
  system_tray_view_->set_items(items);
  system_tray_view_->set_system_tray_type(system_tray_type);

  CreateItemViews(Shell::Get()->session_controller()->login_status());

  // Close bubble view if we failed to create the item view.
  if (!bubble_view_->has_children()) {
    Close();
    return;
  }

  UpdateBottomPadding();

  // Enfore relayout of |bubble_view_|. The view code will skip the relayout of
  // |bubble_view_| if its bounds does not change. However, we need to
  // force |bubble_view_| relayout in order to set the bounds of its newly
  // created children view to preferred sizes.
  bubble_view_->InvalidateLayout();
  bubble_view_->GetWidget()->GetContentsView()->Layout();

  // Make sure that the bubble is large enough for the default view.
  if (system_tray_type == SystemTrayView::SYSTEM_TRAY_TYPE_DEFAULT) {
    bubble_view_->SetMaxHeight(0);  // Clear max height limit.
  }

  if (scoped_layer) {
    // When transitioning from default view to detailed view, animate the new
    // view (slide in from the right).
    if (system_tray_type == SystemTrayView::SYSTEM_TRAY_TYPE_DETAILED) {
      ui::Layer* new_layer = bubble_view_->layer();

      // Make sure the new layer is stacked above the old layer during the
      // animation.
      new_layer->parent()->StackAbove(new_layer, scoped_layer.get());

      gfx::Rect bounds = new_layer->bounds();
      gfx::Transform transform;
      transform.Translate(bounds.width(), 0.0);
      new_layer->SetTransform(transform);
      {
        ui::ScopedLayerAnimationSettings settings(new_layer->GetAnimator());
        settings.AddObserver(
            new AnimationObserverDeleteLayer(scoped_layer.release()));
        settings.SetTransitionDuration(
            base::TimeDelta::FromMilliseconds(kSwipeDelayMS));
        settings.SetTweenType(gfx::Tween::EASE_OUT);
        new_layer->SetTransform(gfx::Transform());
      }
    }
  }

  // Update bubble as size might have changed.
  bubble_view_->UpdateBubble();
}

void SystemTrayBubble::InitView(views::View* anchor,
                                const std::vector<SystemTrayItem*>& items,
                                SystemTrayView::SystemTrayType system_tray_type,
                                LoginStatus login_status,
                                TrayBubbleView::InitParams* init_params) {
  DCHECK(anchor);
  DCHECK(!bubble_view_);

  if (system_tray_type == SystemTrayView::SYSTEM_TRAY_TYPE_DETAILED &&
      init_params->max_height < GetDetailedBubbleMaxHeight()) {
    init_params->max_height = GetDetailedBubbleMaxHeight();
  }

  system_tray_view_ = new SystemTrayView(tray_, system_tray_type, items);

  init_params->delegate = tray_;
  // Place the bubble on same display as this system tray.
  init_params->parent_window = tray_->GetBubbleWindowContainer();
  init_params->anchor_view = anchor;
  bubble_view_ = new TrayBubbleView(*init_params);
  if (features::IsSystemTrayUnifiedEnabled())
    bubble_view_->set_color(kUnifiedMenuBackgroundColor);
  bubble_view_->AddChildView(system_tray_view_);
  UpdateBottomPadding();
  bubble_view_->set_adjust_if_offscreen(false);
  CreateItemViews(login_status);

  if (bubble_view_->CanActivate()) {
    bubble_view_->NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
  }
}

void SystemTrayBubble::BubbleViewDestroyed() {
  bubble_view_ = nullptr;
  system_tray_view_ = nullptr;
}

void SystemTrayBubble::StartAutoCloseTimer(int seconds) {
  autoclose_.Stop();
  autoclose_delay_ = seconds;
  if (autoclose_delay_) {
    autoclose_.Start(FROM_HERE, base::TimeDelta::FromSeconds(autoclose_delay_),
                     this, &SystemTrayBubble::Close);
  }
}

void SystemTrayBubble::StopAutoCloseTimer() {
  autoclose_.Stop();
}

void SystemTrayBubble::RestartAutoCloseTimer() {
  if (autoclose_delay_)
    StartAutoCloseTimer(autoclose_delay_);
}

void SystemTrayBubble::Close() {
  tray_->HideBubbleWithView(bubble_view());
}

void SystemTrayBubble::SetVisible(bool is_visible) {
  if (!bubble_view_)
    return;
  views::Widget* bubble_widget = bubble_view_->GetWidget();
  if (is_visible)
    bubble_widget->Show();
  else
    bubble_widget->Hide();
}

bool SystemTrayBubble::IsVisible() {
  return bubble_view() && bubble_view()->GetWidget()->IsVisible();
}

bool SystemTrayBubble::ShouldShowShelf() const {
  const std::vector<ash::SystemTrayItem*>& items = system_tray_view_->items();
  for (ash::SystemTrayItem* const it : items) {
    if (it->ShouldShowShelf())
      return true;
  }
  return false;
}

void SystemTrayBubble::UpdateBottomPadding() {
  if (system_tray_view_->system_tray_type() ==
      SystemTrayView::SYSTEM_TRAY_TYPE_DEFAULT)
    bubble_view_->SetBottomPadding(kDefaultViewBottomPadding);
  else
    bubble_view_->SetBottomPadding(0);
}

void SystemTrayBubble::CreateItemViews(LoginStatus login_status) {
  bool focused = system_tray_view_->CreateItemViews(login_status);
  if (focused)
    tray_->ActivateBubble();
}

}  // namespace ash
