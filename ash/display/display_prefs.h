// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_PREFS_H_
#define ASH_DISPLAY_DISPLAY_PREFS_H_

#include <stdint.h>
#include <array>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "base/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"

class PrefRegistrySimple;
class PrefService;

namespace gfx {
class Point;
}

namespace display {
struct MixedMirrorModeParams;
struct TouchCalibrationData;
}  // namespace display

namespace ash {

class DisplayPrefsTest;

// Manages display preference settings. Settings are stored in the local state
// for the session.
class ASH_EXPORT DisplayPrefs : public SessionObserver {
 public:
  // Registers the prefs associated with display settings.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  explicit DisplayPrefs(PrefService* local_state);
  ~DisplayPrefs() override;

  // SessionObserver:
  void OnFirstSessionStarted() override;

  // Stores all current displays preferences or queues a request until
  // LoadDisplayPreferences is called.
  void MaybeStoreDisplayPrefs();

  // Test helper methods.
  void StoreDisplayRotationPrefsForTest(display::Display::Rotation rotation,
                                        bool rotation_lock);
  void StoreDisplayLayoutPrefForTest(const display::DisplayIdList& list,
                                     const display::DisplayLayout& layout);
  void StoreDisplayPowerStateForTest(chromeos::DisplayPowerState power_state);
  void LoadTouchAssociationPreferenceForTest();
  void StoreLegacyTouchDataForTest(int64_t display_id,
                                   const display::TouchCalibrationData& data);
  // Parses the marshalled string data stored in local preferences for
  // calibration points and populates |point_pair_quad| using the unmarshalled
  // data. See TouchCalibrationData in Managed display info.
  bool ParseTouchCalibrationStringForTest(
      const std::string& str,
      std::array<std::pair<gfx::Point, gfx::Point>, 4>* point_pair_quad);

  // Stores the given |mixed_params| for tests. Clears stored parameters if
  // |mixed_params| is null.
  void StoreDisplayMixedMirrorModeParamsForTest(
      const base::Optional<display::MixedMirrorModeParams>& mixed_params);

 protected:
  friend class DisplayPrefsTest;

  // Loads display preferences from |local_state_|.
  void LoadDisplayPreferences();

  // Sets |local_state| for testing.
  void SetPrefServiceForTest(PrefService* local_state);

 private:
  PrefService* local_state_ = nullptr;  // Non-owned and must out-live this.
  bool store_requested_ = false;

  DISALLOW_COPY_AND_ASSIGN(DisplayPrefs);
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_PREFS_H_
