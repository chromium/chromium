// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/tablet_volume_controller.h"

#include "ash/constants/ash_switches.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {
namespace {

// Path of the json file that contains side volume button location info.
constexpr char kSideVolumeButtonLocationFilePath[] =
    "/usr/share/chromeos-assets/side_volume_button/location.json";

// The interval between two volume control actions within one volume adjust.
constexpr base::TimeDelta kVolumeAdjustTimeout = base::Seconds(2);

void RecordTabletVolumeAdjustTypeHistogram(TabletModeVolumeAdjustType type) {
  UMA_HISTOGRAM_ENUMERATION(kTabletCountOfVolumeAdjustType, type);
}

}  // namespace

const char kTabletCountOfVolumeAdjustType[] = "Tablet.CountOfVolumeAdjustType";

// Fields of the side volume button location info.
const char kVolumeButtonRegion[] = "region";
const char kVolumeButtonSide[] = "side";

// Values of kVolumeButtonRegion.
const char kVolumeButtonRegionKeyboard[] = "keyboard";
const char kVolumeButtonRegionScreen[] = "screen";
// Values of kVolumeButtonSide.
const char kVolumeButtonSideLeft[] = "left";
const char kVolumeButtonSideRight[] = "right";
const char kVolumeButtonSideTop[] = "top";
const char kVolumeButtonSideBottom[] = "bottom";

TabletVolumeController::TabletVolumeController()
    : side_volume_button_location_file_path_(
          base::FilePath(kSideVolumeButtonLocationFilePath)) {
  ParseSideVolumeButtonLocationInfo();
}

TabletVolumeController::~TabletVolumeController() = default;

void TabletVolumeController::ParseSideVolumeButtonLocationInfo() {
  std::string location_info;
  const base::CommandLine* cl = base::CommandLine::ForCurrentProcess();
  if (cl->HasSwitch(switches::kAshSideVolumeButtonPosition)) {
    location_info =
        cl->GetSwitchValueASCII(switches::kAshSideVolumeButtonPosition);
  } else if (!base::PathExists(side_volume_button_location_file_path_) ||
             !base::ReadFileToString(side_volume_button_location_file_path_,
                                     &location_info) ||
             location_info.empty()) {
    return;
  }

  std::optional<base::Value> parsed_json =
      base::JSONReader::Read(location_info);
  if (!parsed_json || !parsed_json->is_dict()) {
    LOG(ERROR) << "JSONReader failed reading side volume button location info: "
               << location_info;
    return;
  }

  const base::Value::Dict& info_in_dict = parsed_json->GetDict();
  const std::string* region = info_in_dict.FindString(kVolumeButtonRegion);
  if (region)
    side_volume_button_location_.region = *region;

  const std::string* side = info_in_dict.FindString(kVolumeButtonSide);
  if (side)
    side_volume_button_location_.side = *side;
}

bool TabletVolumeController::IsValidSideVolumeButtonLocation() const {
  const std::string region = side_volume_button_location_.region;
  const std::string side = side_volume_button_location_.side;
  if (region != kVolumeButtonRegionKeyboard &&
      region != kVolumeButtonRegionScreen) {
    return false;
  }
  if (side != kVolumeButtonSideLeft && side != kVolumeButtonSideRight &&
      side != kVolumeButtonSideTop && side != kVolumeButtonSideBottom) {
    return false;
  }
  return true;
}

bool TabletVolumeController::ShouldSwapSideVolumeButtons(
    int source_device_id) const {
  if (!IsInternalKeyboardOrUncategorizedDevice(source_device_id))
    return false;

  if (!IsValidSideVolumeButtonLocation())
    return false;

  chromeos::OrientationType screen_orientation =
      Shell::Get()->screen_orientation_controller()->GetCurrentOrientation();
  const std::string side = side_volume_button_location_.side;
  const bool is_landscape_secondary_or_portrait_primary =
      screen_orientation == chromeos::OrientationType::kLandscapeSecondary ||
      screen_orientation == chromeos::OrientationType::kPortraitPrimary;

  if (side_volume_button_location_.region == kVolumeButtonRegionKeyboard) {
    if (side == kVolumeButtonSideLeft || side == kVolumeButtonSideRight)
      return chromeos::IsPrimaryOrientation(screen_orientation);
    return is_landscape_secondary_or_portrait_primary;
  }

  DCHECK_EQ(kVolumeButtonRegionScreen, side_volume_button_location_.region);
  if (side == kVolumeButtonSideLeft || side == kVolumeButtonSideRight)
    return !chromeos::IsPrimaryOrientation(screen_orientation);
  return is_landscape_secondary_or_portrait_primary;
}

void TabletVolumeController::UpdateTabletModeVolumeAdjustHistogram() {
  const int volume_percent = CrasAudioHandler::Get()->GetOutputVolumePercent();
  if ((volume_adjust_starts_with_up_ &&
       volume_percent >= initial_volume_percent_) ||
      (!volume_adjust_starts_with_up_ &&
       volume_percent <= initial_volume_percent_)) {
    RecordTabletVolumeAdjustTypeHistogram(
        TabletModeVolumeAdjustType::kNormalAdjustWithSwapEnabled);
  } else {
    RecordTabletVolumeAdjustTypeHistogram(
        TabletModeVolumeAdjustType::kAccidentalAdjustWithSwapEnabled);
  }
}

bool TabletVolumeController::IsInternalKeyboardOrUncategorizedDevice(
    int source_device_id) const {
  if (source_device_id == ui::ED_UNKNOWN_DEVICE)
    return false;

  for (const ui::InputDevice& keyboard :
       ui::DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard.type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL &&
        keyboard.id == source_device_id) {
      return true;
    }
  }

  for (const ui::InputDevice& uncategorized_device :
       ui::DeviceDataManager::GetInstance()->GetUncategorizedDevices()) {
    if (uncategorized_device.id == source_device_id &&
        uncategorized_device.type ==
            ui::InputDeviceType::INPUT_DEVICE_INTERNAL) {
      return true;
    }
  }
  return false;
}

void TabletVolumeController::StartTabletModeVolumeAdjustTimer(
    bool is_volume_up) {
  if (!tablet_mode_volume_adjust_timer_.IsRunning()) {
    volume_adjust_starts_with_up_ = is_volume_up;
    initial_volume_percent_ = CrasAudioHandler::Get()->GetOutputVolumePercent();
  }
  tablet_mode_volume_adjust_timer_.Start(
      FROM_HERE, kVolumeAdjustTimeout, this,
      &TabletVolumeController::UpdateTabletModeVolumeAdjustHistogram);
}

bool TabletVolumeController::TriggerTabletModeVolumeAdjustTimerForTest() {
  if (!tablet_mode_volume_adjust_timer_.IsRunning())
    return false;

  tablet_mode_volume_adjust_timer_.FireNow();
  return true;
}

void TabletVolumeController::SetSideVolumeButtonFilePathForTest(
    base::FilePath path) {
  side_volume_button_location_file_path_ = path;
  ParseSideVolumeButtonLocationInfo();
}

void TabletVolumeController::SetSideVolumeButtonLocationForTest(
    const std::string& region,
    const std::string& side) {
  side_volume_button_location_.region = region;
  side_volume_button_location_.side = side;
}

const TabletVolumeController::SideVolumeButtonLocation&
TabletVolumeController::GetSideVolumeButtonLocationForTest() const {
  return side_volume_button_location_;
}

}  // namespace ash