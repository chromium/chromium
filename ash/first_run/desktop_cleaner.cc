// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/first_run/desktop_cleaner.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "base/stl_util.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_blocker.h"

namespace ash {
namespace {

std::vector<int> GetContainerIdsToHide() {
  return std::vector<int>{
      // Hide the active desk container. The inactive ones are already hidden.
      // TODO(afakhry): Define the behavior of Virtual Desks during the first
      // run tutorial whether it should be disabled or locked to the currently
      // active desk.
      desks_util::GetActiveDeskContainerId(),

      kShellWindowId_AlwaysOnTopContainer,
      // TODO(dzhioev): uncomment this when issue with BrowserView::CanActivate
      // will be fixed.
      // kShellWindowId_SystemModalContainer
  };
}

}  // namespace

class ContainerHider : public aura::WindowObserver,
                       public ui::ImplicitAnimationObserver {
 public:
  explicit ContainerHider(aura::Window* container)
      : container_was_hidden_(!container->IsVisible()), container_(container) {
    if (container_was_hidden_)
      return;
    ui::Layer* layer = container_->layer();
    ui::ScopedLayerAnimationSettings animation_settings(layer->GetAnimator());
    animation_settings.AddObserver(this);
    layer->SetOpacity(0.0);
  }

  ~ContainerHider() override {
    if (container_was_hidden_ || !container_)
      return;
    if (!WasAnimationCompletedForProperty(ui::LayerAnimationElement::OPACITY)) {
      // We are in the middle of animation.
      StopObservingImplicitAnimations();
    } else {
      container_->Show();
    }
    ui::Layer* layer = container_->layer();
    ui::ScopedLayerAnimationSettings animation_settings(layer->GetAnimator());
    layer->SetOpacity(1.0);
  }

 private:
  // Overriden from ui::ImplicitAnimationObserver.
  void OnImplicitAnimationsCompleted() override { container_->Hide(); }

  // Overriden from aura::WindowObserver.
  void OnWindowDestroying(aura::Window* window) override {
    DCHECK(window == container_);
    container_ = NULL;
  }

  const bool container_was_hidden_;
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(ContainerHider);
};

class NotificationBlocker : public message_center::NotificationBlocker {
 public:
  NotificationBlocker()
      : message_center::NotificationBlocker(
            message_center::MessageCenter::Get()) {
    NotifyBlockingStateChanged();
  }

  ~NotificationBlocker() override = default;

 private:
  // Overriden from message_center::NotificationBlocker.
  bool ShouldShowNotificationAsPopup(
      const message_center::Notification& notification) const override {
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(NotificationBlocker);
};

DesktopCleaner::DesktopCleaner() {
  // TODO(dzhioev): Add support for secondary displays.
  aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
  for (int id : GetContainerIdsToHide()) {
    aura::Window* container = Shell::GetContainer(root_window, id);
    container_hiders_.push_back(std::make_unique<ContainerHider>(container));
  }
  notification_blocker_.reset(new NotificationBlocker());
}

DesktopCleaner::~DesktopCleaner() = default;

// static
std::vector<int> DesktopCleaner::GetContainersToHideForTest() {
  return GetContainerIdsToHide();
}

}  // namespace ash
