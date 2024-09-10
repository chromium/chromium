// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_SCRIM_H_
#define ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_SCRIM_H_

#include <map>
#include <memory>

#include "ash/shell_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class Shell;

// The class, owned by the `WelcomeTourController`, which applies a scrim to the
// help bubble container on all root windows while in existence. On destruction,
// scrims are automatically removed. Only a single `WelcomeTourScrim` instance
// may exist at a time, and the `WelcomeTourController` is responsible for
// ensuring existence if and only if the Welcome Tour is in progress.
class ASH_EXPORT WelcomeTourScrim : public ShellObserver {
 public:
  // Names for layers so they are easy to distinguish in debugging/testing.
  static constexpr char kLayerName[] = "WelcomeTourScrim";
  static constexpr char kMaskLayerName[] = "WelcomeTourScrim::Mask";

  WelcomeTourScrim();
  WelcomeTourScrim(const WelcomeTourScrim&) = delete;
  WelcomeTourScrim& operator=(const WelcomeTourScrim&) = delete;
  ~WelcomeTourScrim() override;

 private:
  class Scrim;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;
  void OnShellDestroying() override;

  // Initializes the scrim for the specified `root_window`.
  void Init(aura::Window* root_window);

  // Resets the scrim for the specified `root_window`.
  void Reset(aura::Window* root_window);

  // Mapping of scrims to their associated root windows.
  std::map<raw_ptr<aura::Window>, std::unique_ptr<Scrim>>
      scrims_by_root_window_;

  // Used to observe `Shell` for the addition/destruction of root windows so
  // that scrims can be created/destroyed appropriately.
  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};
};

}  // namespace ash

#endif  // ASH_USER_EDUCATION_WELCOME_TOUR_WELCOME_TOUR_SCRIM_H_
