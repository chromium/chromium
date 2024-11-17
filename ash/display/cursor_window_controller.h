// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_CURSOR_WINDOW_CONTROLLER_H_
#define ASH_DISPLAY_CURSOR_WINDOW_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/constants/ash_constants.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/display/display.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class CursorWindowControllerTest;
class CursorWindowDelegate;

// Draws a mouse cursor on a given container window.
// When cursor compositing is disabled, draw nothing as the native cursor is
// shown.
// When cursor compositing is enabled, just draw the cursor as-is.
class ASH_EXPORT CursorWindowController : public aura::WindowObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCursorCompositingStateChanged(bool enabled) = 0;

   protected:
    ~Observer() override = default;
  };

  CursorWindowController();

  CursorWindowController(const CursorWindowController&) = delete;
  CursorWindowController& operator=(const CursorWindowController&) = delete;

  ~CursorWindowController() override;

  bool is_cursor_compositing_enabled() const {
    return is_cursor_compositing_enabled_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetLargeCursorSizeInDip(int large_cursor_size_in_dip);
  void SetCursorColor(SkColor cursor_color);

  // If at least one of the features that use cursor compositing is enabled, it
  // should not be disabled. Future features that require cursor compositing
  // should be added in this function.
  bool ShouldEnableCursorCompositing();

  // Sets cursor compositing mode on/off.
  void SetCursorCompositingEnabled(bool enabled);

  // Updates the container window for the cursor window controller.
  void UpdateContainer();

  // Sets the display on which to draw cursor.
  // Only applicable when cursor compositing is enabled.
  void SetDisplay(const display::Display& display);

  // When the mouse starts or stops hovering/resizing the docked magnifier
  // separator, update the container that holds the cursor (so that the cursor
  // is shown on top of the docked magnifier viewport when hovering/resizing).
  // |is_active| is true when user starts hovering the separator.
  // |is_active| is false when user stops hovering and is no longer resizing.
  void OnDockedMagnifierResizingStateChanged(bool is_active);

  // When entering/exiting fullscreen magnifier, reset the container and
  // switch between cursor view and cursor aura window depends on the status.
  void OnFullscreenMagnifierEnabled(bool enabled);

  // Sets cursor location, shape, set and visibility.
  void UpdateLocation();
  void SetCursor(gfx::NativeCursor cursor);
  void SetCursorSize(ui::CursorSize cursor_size);
  void SetVisibility(bool visible);

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Gets the cursor container for testing purposes.
  const aura::Window* GetContainerForTest() const;
  SkColor GetCursorColorForTest() const;
  gfx::Rect GetCursorBoundsInScreenForTest() const;
  const aura::Window* GetCursorHostWindowForTest() const;

 private:
  friend class CursorWindowControllerTest;
  friend class MirrorWindowTestApi;

  // Sets the container window for the cursor window controller.
  // Closes the cursor window if |container| is NULL.
  void SetContainer(aura::Window* container);

  // Sets the bounds of the container in screen coordinates and rotation.
  void SetBoundsInScreenAndRotation(const gfx::Rect& bounds,
                                    display::Display::Rotation rotation);

  // Updates cursor image based on current cursor state.
  void UpdateCursorImage();

  // Hides/shows cursor window based on current cursor state.
  void UpdateCursorVisibility();

  // Updates cursor view based on current cursor state.
  void UpdateCursorView();

  // Update cursor aura window.
  void UpdateCursorWindow();

  const gfx::ImageSkia& GetCursorImageForTest() const;

  // Determines if fast ink cursor should be used.
  bool ShouldUseFastInk() const;

  // If using fast ink, create `cursor_view_widget_`; otherwise,
  // create `cursor_window_`.
  void UpdateCursorMode();

  base::ObserverList<Observer> observers_;

  raw_ptr<aura::Window, DanglingUntriaged> container_ = nullptr;

  // The current cursor-compositing state.
  bool is_cursor_compositing_enabled_ = false;

  // The bounds of the container in screen coordinates.
  gfx::Rect bounds_in_screen_;

  // The rotation of the container.
  display::Display::Rotation rotation_ = display::Display::ROTATE_0;

  // The native cursor, see definitions in cursor.h
  gfx::NativeCursor cursor_ = ui::mojom::CursorType::kNone;

  // The last requested cursor visibility.
  bool visible_ = true;

  ui::CursorSize cursor_size_ = ui::CursorSize::kNormal;
  gfx::Point hot_point_;

  int large_cursor_size_in_dip_ = kDefaultLargeCursorSize;
  SkColor cursor_color_ = kDefaultCursorColor;

  // The display on which the cursor is drawn.
  // For mirroring mode, the display is always the primary display.
  display::Display display_;

  // When using software compositing, cursor_window_ will be used to paint
  // cursor and composited with other elements by ui compositor.
  std::unique_ptr<aura::Window> cursor_window_;
  std::unique_ptr<CursorWindowDelegate> delegate_;
  // When using fast ink, cursor_view_widget_ draws cursor image
  // directly to the front buffer that is overlay candidate.
  views::UniqueWidgetPtr cursor_view_widget_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      scoped_container_observer_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_CURSOR_WINDOW_CONTROLLER_H_
