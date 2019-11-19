// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
#define ASH_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

namespace ash {

// DisplayConfigurationObserver observes and saves
// the change of display configurations.
class ASH_EXPORT DisplayConfigurationObserver
    : public WindowTreeHostManager::Observer,
      public TabletModeObserver {
 public:
  DisplayConfigurationObserver();
  ~DisplayConfigurationObserver() override;

  bool save_preference() const { return save_preference_; }

 protected:
  // WindowTreeHostManager::Observer:
  void OnDisplaysInitialized() override;
  void OnDisplayConfigurationChanged() override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;

 private:
  void StartMirrorMode();
  void EndMirrorMode();

  bool save_preference_ = true;

  // True if the device was in mirror mode before siwtching to tablet mode.
  bool was_in_mirror_mode_ = false;

  base::WeakPtrFactory<DisplayConfigurationObserver> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DisplayConfigurationObserver);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONFIGURATION_OBSERVER_H_
