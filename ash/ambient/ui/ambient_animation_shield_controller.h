// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_SHIELD_CONTROLLER_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_SHIELD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/style/color_mode_observer.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace views {
class View;
}  // namespace views

namespace ash {

// Adds a view containing a dark-mode "shield" to the UI hierarchy whenever dark
// mode is active, and removes it whenever light mode is active. The "shield"
// view's details are up to the caller but is intended to paint some form of
// a "dark" layer on top of the actual contents only when dark mode is active.
//
// The shield UX requirements for animated mode do not overlap enough with the
// gradient-based AmbientShieldView to be re-used.
class ASH_EXPORT AmbientAnimationShieldController : public ColorModeObserver {
 public:
  AmbientAnimationShieldController(std::unique_ptr<views::View> shield_view,
                                   views::View* parent_view);
  AmbientAnimationShieldController(const AmbientAnimationShieldController&) =
      delete;
  AmbientAnimationShieldController& operator=(
      const AmbientAnimationShieldController&) = delete;
  ~AmbientAnimationShieldController() override;

 private:
  void OnColorModeChanged(bool dark_mode_enabled) override;

  const std::unique_ptr<views::View> shield_view_;
  const raw_ptr<views::View> parent_view_;
  base::ScopedObservation<DarkLightModeControllerImpl, ColorModeObserver>
      color_provider_observer_{this};
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_SHIELD_CONTROLLER_H_
