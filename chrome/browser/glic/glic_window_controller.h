// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/views/widget/unique_widget_ptr.h"

class Browser;
namespace gfx {
class Size;
}  // namespace gfx

namespace {
class WindowEventObserver;
}  // namespace

namespace glic {
class GlicView;

// Class for Glic window controller. Owned by the Glic profile keyed-service.
// This gets created when the Glic window needs to be shown and it owns the Glic
// widget.
class GlicWindowController {
 public:
  GlicWindowController(const GlicWindowController&) = delete;
  GlicWindowController& operator=(const GlicWindowController&) = delete;

  explicit GlicWindowController(Profile* profile);
  ~GlicWindowController();

  // Shows the glic window.
  void Show(const views::View* glic_button_view);

  // Sets the size of the glic window to the specified dimensions. Returns true
  // if the operation succeeded.
  bool Resize(const gfx::Size& size);

  // Returns the current size of the glic window.
  gfx::Size GetSize();

  // Called to notify the controller that the window was requested to be closed.
  void Close();

  // User drags glic window
  void DragFromPoint(gfx::Vector2d mouse_location);

  // Returns a WeakPtr to this instance. It can be destroyed at any time if the
  // profile is deleted or if the browser shuts down.
  base::WeakPtr<GlicWindowController> GetWeakPtr();

 private:
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
    raw_ptr<glic::GlicView> glic_view_;
    raw_ptr<views::Widget> pinned_target_widget_;
  };

  PinnedTargetWidgetObserver pinned_target_widget_observer_{this};

  // If the mouse is in snapping distance of a browser's glic button, it snaps
  // glic to the top right of the browser's glic button.
  void HandleBrowserPinning(gfx::Vector2d mouse_location);

  // When glic is unpinned, reparent to empty holder widget. Initializes the
  // empty holder widget if it hasn't been created yet.g
  void MaybeCreateHolderWindowAndReparent();

  // Moves glic view to the pin target of the specified browser.
  void MoveToBrowserPinTarget(Browser* browser);

  // Empty holder widget to reparent to when unpinned.
  std::unique_ptr<views::Widget> holder_widget_;

  const raw_ptr<Profile> profile_;
  views::UniqueWidgetPtr widget_;
  // Owned by widget_.
  raw_ptr<glic::GlicView> glic_view_ = nullptr;

  // Used to monitor key and mouse events from native window.
  std::unique_ptr<WindowEventObserver> window_event_observer_;

  // True while RunMoveLoop() has been called on a widget.
  bool in_move_loop_ = false;

  base::WeakPtrFactory<GlicWindowController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_WINDOW_CONTROLLER_H_
