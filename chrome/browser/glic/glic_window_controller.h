// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "ui/views/widget/unique_widget_ptr.h"

class Browser;
namespace gfx {
class Size;
class Point;
}  // namespace gfx

namespace {
class GlicWidgetObserver;
class WindowEventObserver;
}  // namespace

namespace glic {
class GlicView;

// Class for Glic window controller. Owned by the Glic profile keyed-service.
// This gets created when the Glic window needs to be shown and it owns the Glic
// widget.
class GlicWindowController : public views::WidgetObserver {
 public:
  // Observes the state of the glic window.
  class StateObserver : public base::CheckedObserver {
   public:
    virtual void PanelStateChanged(const mojom::PanelState& panel_state) = 0;
  };

  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;

  explicit GlicWindowController(Profile* profile);
  ~GlicWindowController() override;

  // Shows the glic window.
  void Show(views::View* glic_button_view);

  // Sets the size of the glic window to the specified dimensions. Returns true
  // if the operation succeeded.
  bool Resize(const gfx::Size& size);

  // Returns the current size of the glic window.
  gfx::Size GetSize();

  // Sets the areas of the view from which it should be draggable.
  void SetDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);

  // Called to notify the controller that the window was requested to be closed.
  void Close();

  // Drags the glic window following the current mouse location with a given
  // offset and checks for pinning points during and after the move loop.
  void HandleWindowDragWithOffset(gfx::Vector2d mouse_offset);

  const mojom::PanelState& GetPanelState() const { return panel_state_; }
  void AddStateObserver(StateObserver* observer);
  void RemoveStateObserver(StateObserver* observer);

  // Returns whether or not the glic window is currently active.
  bool IsActive();

  // Whether there is a glic window, regardless of it's visibility to the user.
  bool HasWindow() const;

  using WindowActivationChangedCallback =
      base::RepeatingCallback<void(bool active)>;

  // Registers |callback| to be called whenever the window activation changes.
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback);

  // Returns a WeakPtr to this instance. It can be destroyed at any time if the
  // profile is deleted or if the browser shuts down.
  base::WeakPtr<GlicWindowController> GetWeakPtr();

  // views::WidgetObserver implementation, monitoring the GlicView.
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

 private:
  GlicView* GetGlicView();
  // Determines the correct position for the glic window when attached to a
  // browser.
  gfx::Point GetTopRightPositionForAttachedWindow(
      views::View* glic_button_view);

  // Determines the correct position for the glic window when in a detached
  // state.
  gfx::Point GetTopRightPositionForDetachedWindow();

  // Reparents the glic widget under the given widget.
  void AttachToBrowser(Browser* browser, views::Widget* widget);

  // observes the pinned target
  class PinnedTargetWidgetObserver : public views::WidgetObserver {
   public:
    explicit PinnedTargetWidgetObserver(
        glic::GlicWindowController* glic_window_controller);
    PinnedTargetWidgetObserver(const PinnedTargetWidgetObserver&) = delete;
    PinnedTargetWidgetObserver& operator=(const PinnedTargetWidgetObserver&) =
        delete;
    ~PinnedTargetWidgetObserver() override;
    void SetPinnedTargetWidget(views::Widget* widget);
    void OnWidgetBoundsChanged(views::Widget* widget,
                               const gfx::Rect& new_bounds) override;
    void OnWidgetDestroying(views::Widget* widget) override;

   private:
    raw_ptr<glic::GlicWindowController> glic_window_controller_;
    raw_ptr<views::Widget> pinned_target_widget_;
  };

  // Helper class for observing activation events from the widget.
  class GlicWidgetObserver : public views::WidgetObserver {
   public:
    explicit GlicWidgetObserver(
        glic::GlicWindowController* glic_window_controller,
        views::Widget* widget);
    GlicWidgetObserver(const GlicWidgetObserver&) = delete;
    GlicWidgetObserver& operator=(const GlicWidgetObserver&) = delete;
    ~GlicWidgetObserver() override;

    void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

   private:
    raw_ptr<glic::GlicWindowController> glic_window_controller_;
    raw_ptr<views::Widget> widget_;
  };

  // If the widget is in snapping distance of a browser's glic button, it snaps
  // glic to the top right of the browser's glic button. Also handles snapping
  // when dragging for magnetism.
  void HandleBrowserPinning(views::Widget* widget);

  // When glic is unpinned, reparent it to an empty holder Widget. Initializes
  // the empty holder widget if it hasn't been created yet.
  void MaybeCreateHolderWindowAndReparent();

  // Moves glic view to the pin target of the specified browser. Animate
  // determines if the move to the pin target is animated or not.
  void MoveToBrowserPinTarget(Browser* browser, bool animate);

  void NotifyIfPanelStateChanged();
  mojom::PanelState ComputePanelState() const;

  PinnedTargetWidgetObserver pinned_target_widget_observer_{this};
  base::WeakPtr<Browser> pinned_browser_;

  // Notifies subscribers of a change to the window activation.
  void NotifyWindowActivationChanged(bool active);

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  // Empty holder widget to reparent to when unpinned and when being dragged.
  std::unique_ptr<views::Widget> holder_widget_;

  const raw_ptr<Profile> profile_;
  views::UniqueWidgetPtr widget_;
  bool widget_visible_ = false;

  // Used to monitor key and mouse events from native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;

  // Used to monitor window activation changes from widget.
  std::unique_ptr<GlicWidgetObserver> glic_widget_observer_;

  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;

  // This is the last panel state sent to observers. It should only be updated
  // in `NotifyIfPanelStateChanged`.
  mojom::PanelState panel_state_;

  base::ObserverList<StateObserver> state_observers_;

  base::WeakPtrFactory<GlicWindowController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
