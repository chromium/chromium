// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_IMPL_H_

#include <memory>

#include "ash/boca/on_task/on_task_pod_controller.h"
#include "ash/boca/on_task/on_task_pod_view.h"
#include "ash/style/icon_button.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"

class Browser;
class ImmersiveRevealedLock;

namespace ash {

// OnTask pod controller implementation for the `OnTaskPodView`. This controller
// implementation also owns the widget that hosts the pod component view.
class OnTaskPodControllerImpl : public OnTaskPodController,
                                public aura::WindowObserver,
                                public WindowStateObserver {
 public:
  explicit OnTaskPodControllerImpl(Browser* browser);
  OnTaskPodControllerImpl(const OnTaskPodControllerImpl&) = delete;
  OnTaskPodControllerImpl& operator=(const OnTaskPodControllerImpl) = delete;
  ~OnTaskPodControllerImpl() override;

  // OnTaskPodController:
  void MaybeNavigateToPreviousPage() override;
  void MaybeNavigateToNextPage() override;
  void ReloadCurrentPage() override;
  void ToggleTabStripVisibility(bool show, bool user_action) override;
  void SetSnapLocation(OnTaskPodSnapLocation snap_location) override;
  void OnPauseModeChanged(bool paused) override;
  void OnPageNavigationContextChanged() override;
  bool CanNavigateToPreviousPage() override;
  bool CanNavigateToNextPage() override;
  bool CanToggleTabStripVisibility() override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // Component accessors used for testing purposes.
  views::Widget* GetPodWidgetForTesting();
  ImmersiveRevealedLock* GetTabStripRevealLockForTesting();
  OnTaskPodSnapLocation GetSnapLocationForTesting() const;

 private:
  // Calculates the OnTask pod widget bounds based on the snap location and
  // the parent window frame header height.
  const gfx::Rect CalculateWidgetBounds();

  // Weak pointer for the Boca app instance that is being interacted with.
  const base::WeakPtr<Browser> browser_;

  // Pod widget that contains the `OnTaskPodView`.
  std::unique_ptr<views::Widget> pod_widget_;

  // Prevents the tab strip from hiding while in immersive fullscreen.
  std::unique_ptr<ImmersiveRevealedLock> tab_strip_reveal_lock_;

  // Height of the window frame header. This is used to track the frame header
  // height when in unlocked mode for consistent positioning in locked mode.
  int frame_header_height_;

  // Snap location for the OnTask pod. Top left by default.
  OnTaskPodSnapLocation pod_snap_location_ = OnTaskPodSnapLocation::kTopLeft;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_POD_CONTROLLER_IMPL_H_
