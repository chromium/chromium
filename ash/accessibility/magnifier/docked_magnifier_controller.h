// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_DOCKED_MAGNIFIER_CONTROLLER_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_DOCKED_MAGNIFIER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/cursor/cursor_size.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/events/event_handler.h"
#include "ui/views/widget/widget_observer.h"

class PrefRegistrySimple;
class PrefService;
class PrefChangeRegistrar;

namespace aura {
class Window;
class WindowTreeHost;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Controls the Docked Magnifier (a.k.a. picture-in-picture magnifier) feature,
// which allocates the top portion of the currently active display as a
// magnified viewport of an area around the point of interest in the screen
// (which follows the cursor location, text input caret location, or focus
// changes). In a multiple display scenario, the magnifier viewport is located
// on the same display as that of the point of interest.
class ASH_EXPORT DockedMagnifierController
    : public SessionObserver,
      public ui::EventHandler,
      public views::WidgetObserver,
      public display::DisplayManagerObserver {
 public:
  DockedMagnifierController();
  DockedMagnifierController(const DockedMagnifierController&) = delete;
  DockedMagnifierController& operator=(const DockedMagnifierController&) =
      delete;
  ~DockedMagnifierController() override;

  // The height of the black separator layer between the magnifier viewport and
  // the rest of the screen.
  static constexpr int kSeparatorHeight = 10;

  // The default value by which the screen height is divided to calculate the
  // height of the magnifier viewport.
  static constexpr float kDefaultScreenHeightDivisor = 3.0f;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Get the Docked Magnifier settings for the current active user prefs.
  bool GetEnabled() const;
  float GetScale() const;
  float GetScreenHeightDivisor() const;

  // Set the Docked Magnifier settings in the current active user prefs.
  void SetEnabled(bool enabled);
  void SetScale(float scale);
  void SetScreenHeightDivisor(float screen_height_divisor);

  // Maps the current scale value to an index in the range between the minimum
  // and maximum scale values, and steps up or down the scale depending on the
  // value of |delta_index|.
  void StepToNextScaleValue(int delta_index);

  void CenterOnPoint(const gfx::Point& point_in_screen);
  void MoveMagnifierToRect(const gfx::Rect& rect_in_screen);
  int GetMagnifierHeightForTesting() const;

  // ash::SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* pref_service) override;
  void OnSigninScreenPrefServiceInitialized(PrefService* prefs) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  // display::DisplayManagerObserver:
  void OnDidApplyDisplayChanges() override;

  // Getters and setters of the enabled status of the Fullscreen Magnifier.
  // We need these so that we can guarantee that both magnifiers are mutually
  // exclusive (i.e. only one can be on at the same time).
  // Note: We can't use the ash::MagnificationController since it's not hooked
  // to the associated user-prefs except when Chrome is running (via
  // MagnificationManager), so we can't assert this behavior in ash_unittests.
  // Keep them public for now.
  // TODO(afakhry): Refactor the Fullscreen Magnifier and remove these
  // functions. https://crbug.com/817157.
  bool GetFullscreenMagnifierEnabled() const;
  void SetFullscreenMagnifierEnabled(bool enabled);

  // Returns the total height of the Docked Magnifier, which is the height of
  // the viewport widget plus the height of the separator, if enabled, or zero
  // if disabled.
  int GetTotalMagnifierHeight() const;

  // Returns the total bounds of the magnifier region (i.e. magnifier viewport
  // plus the separator) if the magnifier is shown for the given |root|. Returns
  // empty bounds otherwise.
  gfx::Rect GetTotalMagnifierBoundsForRoot(aura::Window* root) const;

  const views::Widget* GetViewportWidgetForTesting() const;

  const ui::Layer* GetViewportMagnifierLayerForTesting() const;

  float GetMinimumPointOfInterestHeightForTesting() const;

 private:
  // If user has large cursor enabled, don't change the cursor size.
  // Otherwise, set the cursor to be the specified size.
  void MaybeSetCursorSize(ui::CursorSize cursor_size);

  // If user is starting or continuing to drag the separator, move the
  // separator and resize the viewport.
  void MaybePerformViewportResizing(ui::MouseEvent* event);

  // Resets the large docked magnifier viewport resizing cursor if
  // it was locked because it was over the separator.
  void MaybeResetResizingCursor();

  // Switches the current source root window to |new_root_window| if it's
  // different than |current_source_root_window_|, destroys (if any) old
  // viewport layers and widgets, and recreates them if |new_root_window| is not
  // |nullptr|.
  // In the event of a display removal in which the magnifier viewport was
  // resident, use |update_old_root_workarea = true| to avoid updating the
  // workarea of a removed display which involves re-layout of the shelf which
  // may have been destroyed.
  void SwitchCurrentSourceRootWindowIfNeeded(aura::Window* new_root_window,
                                             bool update_old_root_workarea);

  void InitFromUserPrefs();

  // Handlers of prefs changes.
  void OnEnabledPrefChanged();
  void OnScalePrefChanged();
  void OnFullscreenMagnifierEnabledPrefChanged();

  void Refresh();

  void NotifyClientWithStatusChanged();

  void CreateMagnifierViewport();

  // Whenever there's a change that affects the screen size, rotation, the
  // current root window or the scale of the magnified view, this function is
  // used to recalculate (if needed) minimum height of the point of interest,
  // which is used to avoid translating the magnifier layer such that magnifier
  // viewport shows a magnified version of itself.
  void MaybeCachePointOfInterestMinimumHeight(aura::WindowTreeHost* host);

  // Prevents the mouse cursor from being able to enter inside the magnifier
  // viewport.
  void ConfineMouseCursorOutsideViewport();

  // Invoked when |move_magnifier_timer_| fires to move the magnifier window to
  // follow the caret.
  void OnMoveMagnifierTimer();

  // Whether or not user has started resizing the Docked Magnifier.
  bool is_resizing_ = false;

  // Vertical offset from user's mouse cursor to the top of separator. Stored to
  // later compute how far offset the new separator position should be based on
  // location user initially clicked separator.
  int resize_offset_ = 0;

  // The current root window of the source display from which we are reflecting
  // and magnifying into the viewport. It is set to |nullptr| when the magnifier
  // is disabled. The viewport is placed on the same display.
  raw_ptr<aura::Window> current_source_root_window_ = nullptr;

  // The height below which the point of interest is not allowed to go. This is
  // so that we can avoid mirroring the magnifier viewport into itself.
  float minimum_point_of_interest_height_ = 0.0f;

  // Indicates the the above value is valid and doesn't need to be recalculated.
  bool is_minimum_point_of_interest_height_valid_ = false;

  // Indicates cursor is locked, because cursor has moved over separator.
  bool is_cursor_locked_ = false;

  // The viewport widget which occupies the top 1/4th of the current display on
  // which it is shown. It contains all the magnifier related layer.
  raw_ptr<views::Widget> viewport_widget_ = nullptr;

  // A solid color layer that shows a dark gray background behind the magnifier
  // layer.
  std::unique_ptr<ui::Layer> viewport_background_layer_;

  // The layer into which the current display's compositor is reflected and
  // magnified. It is transformed such that only the area around the point of
  // interest shows up in the viewport.
  std::unique_ptr<ui::Layer> viewport_magnifier_layer_;

  // A solid color layer that shows a black line separating the magnifier
  // viewport from the rest of the display contents.
  std::unique_ptr<ui::Layer> separator_layer_;

  // The pref service of the currently active user. Can be null in
  // ash_unittests.
  raw_ptr<PrefService> active_user_pref_service_ = nullptr;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Timer for moving magnifier window when it fires.
  base::OneShotTimer move_magnifier_timer_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_DOCKED_MAGNIFIER_CONTROLLER_H_
