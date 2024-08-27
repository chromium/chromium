// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_VIEWS_WALLPAPER_WIDGET_CONTROLLER_H_
#define ASH_WALLPAPER_VIEWS_WALLPAPER_WIDGET_CONTROLLER_H_

#include <list>
#include <memory>

#include "ash/ash_export.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display_observer.h"

namespace ui {
class Layer;
class LayerTreeOwner;
}  // namespace ui

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {
class WallpaperView;

// This class manages widget-based wallpapers.
// WallpaperWidgetController is owned by RootWindowController.
class ASH_EXPORT WallpaperWidgetController
    : public ui::ImplicitAnimationObserver,
      public display::DisplayObserver,
      public ui::ColorProviderSourceObserver {
 public:
  explicit WallpaperWidgetController(aura::Window* root_window);

  WallpaperWidgetController(const WallpaperWidgetController&) = delete;
  WallpaperWidgetController& operator=(const WallpaperWidgetController&) =
      delete;

  ~WallpaperWidgetController() override;

  WallpaperView* wallpaper_view() { return wallpaper_view_; }

  ui::Layer* wallpaper_underlay_layer() {
    return wallpaper_underlay_layer_.get();
  }

  // Initializes the widget. `locked` determines if the wallpaper should be
  // created for the locked state.
  void Init(bool locked);

  views::Widget* GetWidget();

  // Returns true if wallpaper change is in progress, i.e. `animating_widget_`
  // exists.
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

  // Sets/Gets the blur used to draw wallpaper. |animation_duration| specifies
  // the animation to apply the change. If its zero duration, then no animation
  // will be applied.
  bool SetWallpaperBlur(
      float blur,
      const base::TimeDelta& animation_duration = base::TimeDelta());
  float GetWallpaperBlur() const;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

  ui::LayerTreeOwner* old_layer_tree_owner_for_testing() {
    return old_layer_tree_owner_.get();
  }

 private:
  void CreateWallpaperUnderlayLayer();

  // Runs callbacks in |animation_end_callbacks_|.
  void RunAnimationEndCallbacks();

  // Copies and fades out the existing wallpaper.
  void ApplyCrossFadeAnimation(base::TimeDelta duration);

  raw_ptr<aura::Window> root_window_;

  // The current wallpaper widget.
  std::unique_ptr<views::Widget> widget_;

  // The animating layer which contains old content. This is the layer that is
  // animated when changing wallpapers.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_tree_owner_;

  // Pointer to the wallpaper view owned by |widget_|.
  raw_ptr<WallpaperView> wallpaper_view_ = nullptr;

  // A solid-color layer stacked below the clipped `wallpaper_view_`
  // layer. Note that it can't be stacked at bottom since the `shield_view_` may
  // exist.
  std::unique_ptr<ui::Layer> wallpaper_underlay_layer_;

  // Callbacks to be run when the |animating_widget_| stops animating and gets
  // set as the active widget.
  std::list<base::OnceClosure> animation_end_callbacks_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_VIEWS_WALLPAPER_WIDGET_CONTROLLER_H_
