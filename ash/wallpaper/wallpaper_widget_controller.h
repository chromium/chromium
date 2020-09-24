// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_
#define ASH_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_

#include <list>
#include <memory>

#include "ash/ash_export.h"
#include "ash/wallpaper/wallpaper_property.h"
#include "base/callback.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"

namespace ui {
class LayerTreeOwner;
}

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
class ASH_EXPORT WallpaperWidgetController
    : public ui::ImplicitAnimationObserver {
 public:
  WallpaperWidgetController(aura::Window* root_window,
                            base::OnceClosure wallpaper_set_callback);
  ~WallpaperWidgetController() override;

  // Initialize the widget. |lock| specifies if the wallpaper should be created
  // for the locked state.
  void Init(bool locked);

  views::Widget* GetWidget();

  // Whether a wallpaper change is in progress, i.e. |animating_widget_| exists.
  bool IsAnimating() const;

  // If an animating wallpaper change is in progress, it ends the animation and
  // changes the wallpaper immediately. No-op if IsAnimation() returns false.
  void StopAnimating();

  // Adds a callback that will be run when the wallpaper animation ends. Used
  // when you're expecting a wallpaper change (e.g. when IsAnimation() returns
  // true or you just set a new wallpaper) and want to be notified of the exact
  // timing that the wallpaper is applied.
  void AddAnimationEndCallback(base::OnceClosure callback);

  // Move the wallpaper widget to the specified |container|.
  // The lock screen moves the wallpaper container to hides the user's windows.
  // Returns true if there was something to reparent.
  bool Reparent(int container);

  // Sets/Gets the properties (blur and opacity) used to draw wallpaper.
  // |animation_duration| specifies the animation to apply the change.
  // If its zero duration, then no animation will be applied.
  bool SetWallpaperProperty(
      const WallpaperProperty& property,
      const base::TimeDelta& animation_duration = base::TimeDelta());
  const WallpaperProperty& GetWallpaperProperty() const;

  WallpaperView* wallpaper_view() const { return wallpaper_view_; }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

 private:
  // Runs callbacks in |animation_end_callbacks_|.
  void RunAnimationEndCallbacks();

  void ApplyCrossFadeAnimation(base::TimeDelta duration);

  aura::Window* root_window_;

  // Callback that will be run when |active_widget_| is first set.
  base::OnceClosure wallpaper_set_callback_;

  // The current wallpaper widget.
  std::unique_ptr<views::Widget> widget_;

  // The animating layer which contains old content.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner_;

  // Pointer to the wallpaper view owned by |widget_|.
  WallpaperView* wallpaper_view_ = nullptr;

  // Callbacks to be run when the |animating_widget_| stops animating and gets
  // set as the active widget.
  std::list<base::OnceClosure> animation_end_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperWidgetController);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_WIDGET_CONTROLLER_H_
