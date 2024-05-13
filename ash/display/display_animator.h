// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_ANIMATOR_H_
#define ASH_DISPLAY_DISPLAY_ANIMATOR_H_

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/display/manager/display_configurator.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

// DisplayAnimator provides the visual effects for
// display::DisplayConfigurator, such like fade-out/in during changing
// the display mode.
class ASH_EXPORT DisplayAnimator
    : public display::DisplayConfigurator::Observer {
 public:
  DisplayAnimator();

  DisplayAnimator(const DisplayAnimator&) = delete;
  DisplayAnimator& operator=(const DisplayAnimator&) = delete;

  ~DisplayAnimator() override;

  void StartFadeOutAnimation(base::OnceClosure callback);
  void StartFadeInAnimation();

 protected:
  // display::DisplayConfigurator::Observer overrides:
  void OnDisplayConfigurationChanged(
      const display::DisplayConfigurator::DisplayStateList& outputs) override;
  void OnDisplayConfigurationChangeFailed(
      const display::DisplayConfigurator::DisplayStateList& displays,
      display::MultipleDisplayState failed_new_state) override;

 private:
  // Clears all hiding layers.  Note that in case that this method is called
  // during an animation, the method call will cancel all of the animations
  // and *not* call the registered callback.
  void ClearHidingLayers();

  std::map<aura::Window*, std::unique_ptr<ui::Layer>> hiding_layers_;
  std::unique_ptr<base::OneShotTimer> timer_;
  base::WeakPtrFactory<DisplayAnimator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_ANIMATOR_H_
