// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SCOPED_WINDOW_TUCKER_H_
#define ASH_WM_SCOPED_WINDOW_TUCKER_H_

#include <memory>

#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/scoped_window_event_targeting_blocker.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {

constexpr char kTuckUserAction[] = "FloatWindowTucked";
constexpr char kUntuckUserAction[] = "FloatWindowUntucked";

// Scoped class which makes modifications while a window is tucked. It owns a
// tuck handle widget that will bring the hidden window back onscreen. Users of
// the class need to ensure that window outlives instance of this class.
class ScopedWindowTucker : public wm::ActivationChangeObserver,
                           public OverviewObserver,
                           public aura::WindowObserver {
 public:
  static constexpr int kTuckHandleWidth = 20;
  static constexpr int kTuckHandleHeight = 92;

  class Delegate {
   public:
    Delegate();
    virtual ~Delegate();

    // Paint the tuck handle.
    virtual void PaintTuckHandle(gfx::Canvas* canvas, int width, bool left) = 0;

    // Returns `kContainerIdsToMove` for the parent of the handle widget.
    virtual int ParentContainerId() const = 0;

    // Updates the position of the window.
    virtual void UpdateWindowPosition(aura::Window* window, bool left) = 0;

    // Destroys `this_`, which will untuck `window_` and set the window bounds
    // back onscreen.
    virtual void UntuckWindow(aura::Window* window) = 0;

    // Hides the window after the tuck animation is finished. This is so it will
    // behave similarly to a minimized window in overview.
    virtual void OnAnimateTuckEnded(aura::Window* window) = 0;

    // Returns proper bounds for tuck handle.
    virtual gfx::Rect GetTuckHandleBounds(
        bool left,
        const gfx::Rect& window_bounds) const = 0;

    base::WeakPtr<Delegate> GetWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  // Represents a tuck handle that untucks floated / PiP windows from offscreen.
  class TuckHandleView : public views::Button,
                         public views::ViewTargeterDelegate {
   public:
    TuckHandleView(base::WeakPtr<Delegate> delegate,
                   base::RepeatingClosure callback,
                   bool left);
    TuckHandleView(const TuckHandleView&) = delete;
    TuckHandleView& operator=(const TuckHandleView&) = delete;
    ~TuckHandleView() override;

    // views::Button:
    void OnThemeChanged() override;
    void PaintButtonContents(gfx::Canvas* canvas) override;
    void OnGestureEvent(ui::GestureEvent* event) override;

    // views::ViewTargeterDelegate:
    bool DoesIntersectRect(const views::View* target,
                           const gfx::Rect& rect) const override;

   private:
    // The delegate held by `ScopedWindowTucker`.
    base::WeakPtr<Delegate> scoped_window_tucker_delegate_;

    // Whether the tuck handle is on the left or right edge of the screen. A
    // left tuck handle will have the chevron arrow pointing right and vice
    // versa.
    const bool left_;
  };

  // Creates an instance for `window` where `left` is the side of the screen
  // that the tuck handle is on.
  explicit ScopedWindowTucker(std::unique_ptr<Delegate> delegate,
                              aura::Window* window,
                              bool left);
  ScopedWindowTucker(const ScopedWindowTucker&) = delete;
  ScopedWindowTucker& operator=(const ScopedWindowTucker&) = delete;
  ~ScopedWindowTucker() override;

  // Returns the target window the resizer was created for.
  aura::Window* window() { return window_; }

  // Returns the tuck handle widget that this tucker manages.
  views::Widget* tuck_handle_widget() { return tuck_handle_widget_.get(); }

  // Returns true if the window is tucked to the left of the screen edge.
  bool left() const { return left_; }

  // Starts the tucking animation.
  void AnimateTuck();

  // Starts the untucking animation. Runs `callback` when the animation
  // is completed.
  void AnimateUntuck(base::OnceClosure callback);

  // Runs `delegate`'s `UntuckWindow()`.
  void UntuckWindow();

  // Runs `delegate`'s `OnAnimateTuckEnded()`.
  void OnAnimateTuckEnded();

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

 private:
  // Initializes the tuck handle widget.
  void InitializeTuckHandleWidget();

  // Slides the tuck handle offscreen and onscreen when entering and exiting
  // overview mode respectively.
  void OnOverviewModeChanged(bool in_overview);

  std::unique_ptr<Delegate> delegate_;

  // The window that is being tucked. Will be tucked and untucked by the tuck
  // handle.
  raw_ptr<aura::Window> window_;

  // True if the window is tucked to the left screen edge, false otherwise.
  bool left_ = false;

  // Blocks events from hitting the window while `this` is alive.
  aura::ScopedWindowEventTargetingBlocker event_blocker_;

  views::UniqueWidgetPtr tuck_handle_widget_ =
      std::make_unique<views::Widget>();

  base::ScopedObservation<OverviewController, OverviewObserver>
      overview_observer_{this};

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};

  base::WeakPtrFactory<ScopedWindowTucker> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_SCOPED_WINDOW_TUCKER_H_
