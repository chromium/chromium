// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_PREFS_H_
#define ASH_DISPLAY_DISPLAY_PREFS_H_

#include <stdint.h>

#include <array>
#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/raw_ptr.h"
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

  DisplayPrefs(const DisplayPrefs&) = delete;
  DisplayPrefs& operator=(const DisplayPrefs&) = delete;

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
  void LoadDisplayPrefsForTest();
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
      const std::optional<display::MixedMirrorModeParams>& mixed_params);

  // Checks if given `display_id` was saved to pref service before. Used to tell
  // if a display is connected for the first time or not.
  bool IsDisplayAvailableInPref(int64_t display_id) const;

 protected:
  friend class DisplayPrefsTest;

  // Loads display preferences from |local_state_|.
  void LoadDisplayPreferences();

 private:
  raw_ptr<PrefService> local_state_;  // Non-owned and must out-live this.
  bool store_requested_ = false;
};

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_PREFS_H_
