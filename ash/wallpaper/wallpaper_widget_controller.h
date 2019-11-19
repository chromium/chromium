// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_
#define ASH_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_

#include <list>
#include <memory>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
class WallpaperView;

// This class manages widget-based wallpapers.
// WallpaperWidgetController is owned by RootWindowController.
// Exported for tests.
class ASH_EXPORT WallpaperWidgetController {
 public:
  explicit WallpaperWidgetController(base::OnceClosure wallpaper_set_callback);
  ~WallpaperWidgetController();

  views::Widget* GetWidget();
  views::Widget* GetAnimatingWidget();

  // Whether a wallpaper change is in progress, i.e. |animating_widget_| exists.
  bool IsAnimating() const;

  // If an animating wallpaper change is in progress, it ends the animation and
  // changes the wallpaper immediately. No-op if IsAnimation() returns false.
  void EndPendingAnimation();

  // Adds a callback that will be run when the wallpaper animation ends. Used
  // when you're expecting a wallpaper change (e.g. when IsAnimation() returns
  // true or you just set a new wallpaper) and want to be notified of the exact
  // timing that the wallpaper is applied.
  void AddAnimationEndCallback(base::OnceClosure callback);

  // Sets a new wallpaper widget - this will not change the primary widget
  // immediately. The primary widget will be switched when |widget|'s showing
  // animation finishes (during which |widget| will be kept by
  // |animating_widget_|).
  // |blur_sigma| - if non-zero, the blur that should be applied to the
  //     wallpaper widget layer.
  void SetWallpaperWidget(views::Widget* widget,
                          WallpaperView* wallpaper_view,
                          float blur_sigma);

  // Move the wallpaper for |root_window| to the specified |container|.
  // The lock screen moves the wallpaper container to hides the user's windows.
  // Returns true if there was something to reparent.
  bool Reparent(aura::Window* root_window, int container);

  // Blur pixels of the wallpaper layer by 3 * the given amount.
  void SetWallpaperBlur(float blur_sigma);

  // TODO: Get the wallpaper view from |animating_widget_| or |active_widget_|
  // instead of caching the pointer value.
  WallpaperView* wallpaper_view() const { return wallpaper_view_; }

  // Reset, and closes both |active_widget_| and |animating_widget_|. Can be
  // used in tests to reset the wallpaper widget controller state.
  void ResetWidgetsForTesting();

 private:
  // Wrapper around wallpaper widgets that manages the widget state.
  class WidgetHandler;

  // Called when a WidgetHandler for a wallpaper widget is reset - this happens
  // when the wallpaper widget is being destroyed.
  void WidgetHandlerReset(WidgetHandler* widget);

  // Called when WidgetHandler |widget| detects its associated widget is done
  // animating to shown state.
  void WidgetFinishedAnimating(WidgetHandler* widget);

  // Moves |animated_widget_| to |active_widget_|.
  void SetAnimatingWidgetAsActive();

  // Runs callbacks in |animation_end_callbacks_|.
  void RunAnimationEndCallbacks();

  // Callback that will be run when |active_widget_| is first set.
  base::OnceClosure wallpaper_set_callback_;

  // The current wallpaper widget.
  std::unique_ptr<WidgetHandler> active_widget_;

  // The pending wallpaper widget, which is currently in process of being
  // shown.
  std::unique_ptr<WidgetHandler> animating_widget_;

  // Pointer to the wallpaper view owned by |animating_widget_| if it exists,
  // otherwise owned by |active_widget_|.
  WallpaperView* wallpaper_view_ = nullptr;

  // Callbacks to be run when the |animating_widget_| stops animating and gets
  // set as the active widget.
  std::list<base::OnceClosure> animation_end_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperWidgetController);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_
