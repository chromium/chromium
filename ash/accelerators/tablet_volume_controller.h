// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_TABLET_VOLUME_CONTROLLER_H_
#define ASH_ACCELERATORS_TABLET_VOLUME_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/files/file_path.h"
#include "base/timer/timer.h"

namespace ash {

// Histogram for volume adjustment in tablet mode.
ASH_EXPORT extern const char kTabletCountOfVolumeAdjustType[];

// Fields of the side volume button location info.
ASH_EXPORT extern const char kVolumeButtonRegion[];
ASH_EXPORT extern const char kVolumeButtonSide[];
// Values of kVolumeButtonRegion.
ASH_EXPORT extern const char kVolumeButtonRegionKeyboard[];
ASH_EXPORT extern const char kVolumeButtonRegionScreen[];
// Values of kVolumeButtonSide.
ASH_EXPORT extern const char kVolumeButtonSideLeft[];
ASH_EXPORT extern const char kVolumeButtonSideRight[];
ASH_EXPORT extern const char kVolumeButtonSideTop[];
ASH_EXPORT extern const char kVolumeButtonSideBottom[];

// See TabletModeVolumeAdjustType at tools/metrics/histograms/enums.xml.
enum class TabletModeVolumeAdjustType {
  kAccidentalAdjustWithSwapEnabled = 0,
  kNormalAdjustWithSwapEnabled = 1,
  kAccidentalAdjustWithSwapDisabled = 2,
  kNormalAdjustWithSwapDisabled = 3,
  kMaxValue = kNormalAdjustWithSwapDisabled,
};

class ASH_EXPORT TabletVolumeController {
 public:
  // Some Chrome OS devices have volume up and volume down buttons on their
  // side. We want the button that's closer to the top/right to increase the
  // volume and the button that's closer to the bottom/left to decrease the
  // volume, so we use the buttons' location and the device orientation to
  // determine whether the buttons should be swapped.
  struct SideVolumeButtonLocation {
    // The button can be at the side of the keyboard or the display. Then value
    // of the region could be kVolumeButtonRegionKeyboard or
    // kVolumeButtonRegionScreen.
    std::string region;
    // Side info of region. The value could be kVolumeButtonSideLeft,
    // kVolumeButtonSideRight, kVolumeButtonSideTop or kVolumeButtonSideBottom.
    std::string side;
  };

  TabletVolumeController();
  TabletVolumeController(const TabletVolumeController&) = delete;
  TabletVolumeController& operator=(const TabletVolumeController&) = delete;
  ~TabletVolumeController();

  // Returns true if |source_device_id| corresponds to the internal keyboard or
  // an internal uncategorized input device.
  bool IsInternalKeyboardOrUncategorizedDevice(int source_device_id) const;

  // Returns true if |side_volume_button_location_| is in agreed format and
  // values.
  bool IsValidSideVolumeButtonLocation() const;

  // Returns true if the side volume buttons should be swapped. See
  // SideVolumeButtonLocation for the details.
  bool ShouldSwapSideVolumeButtons(int source_device_id) const;

  // Read the side volume button location info from local file under
  // kSideVolumeButtonLocationFilePath, parse and write it into
  // |side_volume_button_location_|.
  void ParseSideVolumeButtonLocationInfo();

  // Starts |tablet_mode_volume_adjust_timer_| while see VOLUME_UP or
  // VOLUME_DOWN acceleration action when in tablet mode.
  void StartTabletModeVolumeAdjustTimer(bool is_volume_up);

  // The metrics recorded include accidental volume adjustments (defined as a
  // sequence of volume button events in close succession starting with a
  // volume-up event but ending with an overall-decreased volume, or vice versa)
  // or normal volume adjustments w/o SwapSideVolumeButtonsForOrientation
  // feature enabled.
  void UpdateTabletModeVolumeAdjustHistogram();

  const SideVolumeButtonLocation& GetSideVolumeButtonLocationForTest() const;
  bool TriggerTabletModeVolumeAdjustTimerForTest();
  void SetSideVolumeButtonFilePathForTest(base::FilePath path);
  void SetSideVolumeButtonLocationForTest(const std::string& region,
                                          const std::string& side);

 private:
  // Path of the file that contains the side volume button location info. It
  // should always be kSideVolumeButtonLocationFilePath. But it is allowed to be
  // set to different paths in test.
  base::FilePath side_volume_button_location_file_path_;

  // Stores the location info of side volume buttons.
  SideVolumeButtonLocation side_volume_button_location_;

  // Started when VOLUME_DOWN or VOLUME_UP accelerator action is seen while in
  // tablet mode. Runs UpdateTabletModeVolumeAdjustHistogram() to record
  // metrics.
  base::OneShotTimer tablet_mode_volume_adjust_timer_;

  // True if volume adjust starts with VOLUME_UP action.
  bool volume_adjust_starts_with_up_ = false;

  // The initial volume percentage when volume adjust starts.
  int initial_volume_percent_ = 0;
};
}  // namespace ash

#endif  // ASH_ACCELERATORS_TABLET_VOLUME_CONTROLLER_H_