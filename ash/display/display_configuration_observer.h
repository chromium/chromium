// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
#define ASH_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace ash {

// DisplayConfigurationObserver observes and saves
// the change of display configurations.
class ASH_EXPORT DisplayConfigurationObserver
    : public display::DisplayManagerObserver,
      public display::DisplayObserver {
 public:
  DisplayConfigurationObserver();

  DisplayConfigurationObserver(const DisplayConfigurationObserver&) = delete;
  DisplayConfigurationObserver& operator=(const DisplayConfigurationObserver&) =
      delete;

  ~DisplayConfigurationObserver() override;

 protected:
  // display::DisplayManagerObserver:
  void OnDisplaysInitialized() override;
  void OnDidApplyDisplayChanges() override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

 private:
  void StartMirrorMode();
  void EndMirrorMode();

  // True if the device was in mirror mode before siwtching to tablet mode.
  bool was_in_mirror_mode_ = false;

  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<DisplayConfigurationObserver> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
