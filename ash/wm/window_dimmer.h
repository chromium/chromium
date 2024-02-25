// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_DIMMER_H_
#define ASH_WM_WINDOW_DIMMER_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_source_observer.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {

// WindowDimmer creates a window whose opacity is optionally animated by way of
// SetDimOpacity() and whose size matches that of its parent. WindowDimmer is
// intended to be used in cases where a certain set of windows need to appear
// partially obscured. This is achieved by creating WindowDimmer, setting the
// opacity, and then stacking window() above the windows that are to appear
// obscured.
//
// WindowDimmer owns the window it creates, but supports having that window
// deleted out from under it (this generally happens if the parent of the
// window is deleted). If WindowDimmer is deleted and the window it created is
// still valid, then WindowDimmer deletes the window.
class ASH_EXPORT WindowDimmer : public wm::ActivationDelegate,
                                public aura::WindowObserver,
                                public ui::ColorProviderSourceObserver {
 public:
  // Defines an interface for an optional delegate to the WindowDimmer, which
  // will be notified with certain events happening to the window being dimmed.
  class Delegate {
   public:
    // Called when the window being dimmed |dimmed_window| is about to be
    // destroyed.
    // This can be used by the owner of the WindowDimmer to know when it's no
    // longer needed and can be destroyed since the window being dimmed itself
    // is destroying.
    virtual void OnDimmedWindowDestroying(aura::Window* dimmed_window) = 0;

    // Called when the window being dimmed |dimmed_window| changes its parent.
    virtual void OnDimmedWindowParentChanged(aura::Window* dimmed_window) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  // Creates a new WindowDimmer. The window() created by WindowDimmer is added
  // to |parent| and stacked above all other child windows. If |animate| is set
  // to false, the dimming |window_| created by |this| will not animate on its
  // visibility changing, otherwise it'll have a fade animation of a 200-ms
  // duration. |delegate| can be optionally specified to observe some events
  // happening to the window being dimmed (|parent|).
  explicit WindowDimmer(aura::Window* parent,
                        bool animate = true,
                        Delegate* delegate = nullptr);

  WindowDimmer(const WindowDimmer&) = delete;
  WindowDimmer& operator=(const WindowDimmer&) = delete;

  ~WindowDimmer() override;

  aura::Window* window() { return window_; }

  // Set the opacity value of the default dimming color which is Black. If it's
  // desired to specify a certain color with its alpha value, then use the below
  // SetDimColor().
  void SetDimOpacity(float target_opacity);

  // Sets the color of the dimming `window_`'s layer based on the given
  // `color_id`. This color must not be opaque.
  void SetDimColor(ui::ColorId color_id);

  // wm::ActivationDelegate:
  bool ShouldActivate() const override;

  // NOTE: WindowDimmer is an observer for both |parent_| and |window_|.
  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowAddedToRootWindow(aura::Window* window) override;

  // ui::ColorProviderSourceObserver:
  void OnColorProviderChanged() override;

 private:
  // Sets / updates the color of the dimming `window_`'s layer based on
  // `dim_color_type_`.
  void UpdateDimColor();

  raw_ptr<aura::Window> parent_;
  // See class description for details on ownership.
  raw_ptr<aura::Window, DanglingUntriaged> window_;

  raw_ptr<Delegate> delegate_;  // Not owned.

  // Used to get the color for the dimming `window_`'s layer. It's updated
  // through `SetDimColor`. It will be reset when SetDimOpacity() is called.
  std::optional<ui::ColorId> dim_color_type_;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_DIMMER_H_
