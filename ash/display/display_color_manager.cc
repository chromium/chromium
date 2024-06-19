// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager.h"

#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/shell.h"
#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/quirks/quirks_manager.h"
#include "third_party/zlib/google/compression_utils.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/skia_color_space_util.h"

namespace ash {

namespace {

// Runs on a background thread because it does file IO.
std::unique_ptr<display::ColorCalibration> ParseDisplayProfileFile(
    const base::FilePath& path,
    bool has_color_correction_matrix) {
  VLOG(1) << "Trying ICC file " << path.value()
          << " has_color_correction_matrix: "
          << (has_color_correction_matrix ? "true" : "false");
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // TODO(b/347862774): Remove callers to this function.
  return nullptr;
}

std::unique_ptr<display::ColorCalibration> ParseVpdEntry(
    bool has_color_correction_matrix) {
  std::optional<std::string_view> display_profile_string =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kDisplayProfilesKey);
  DCHECK(display_profile_string);
  // Remove hex product code and colon delimiter.
  display_profile_string->remove_prefix(9);

  std::string input;
  if (!base::Base64Decode(display_profile_string.value(), &input)) {
    LOG(WARNING) << "Failed to decode vpd display_profiles entry.";
    return nullptr;
  }

  std::string output;
  if (!compression::GzipUncompress(input, &output)) {
    LOG(WARNING) << "Failed to decompress vpd display_profiles entry.";
    return nullptr;
  }

  // TODO(b/347862774): Remove callers to this function.
  return nullptr;
}

bool HasColorCorrectionMatrix(display::DisplayConfigurator* configurator,
                              int64_t display_id) {
  for (const display::DisplaySnapshot* display_snapshot :
       configurator->cached_displays()) {
    if (display_snapshot->display_id() != display_id)
      continue;

    return display_snapshot->has_color_correction_matrix();
  }

  return false;
}

}  // namespace

DisplayColorManager::DisplayColorManager(
    display::DisplayConfigurator* configurator)
    : configurator_(configurator),
      matrix_buffer_(9, 0.0f),  // 3x3 matrix.
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      displays_ctm_support_(DisplayCtmSupport::kNone) {
  configurator_->AddObserver(this);
}

DisplayColorManager::~DisplayColorManager() {
  configurator_->RemoveObserver(this);
}

bool DisplayColorManager::SetDisplayColorTemperatureAdjustment(
    int64_t display_id,
    const display::ColorTemperatureAdjustment& cta) {
  if (!HasColorCorrectionMatrix(configurator_, display_id)) {
    // This display doesn't support setting a CRTC matrix.
    return false;
  }
  configurator_->SetColorTemperatureAdjustment(display_id, cta);

  // Always overwrite any existing matrix for this display.
  SkM44 color_matrix = gfx::SkM44FromSkcmsMatrix3x3(cta.srgb_matrix);
  displays_color_matrix_map_[display_id] = color_matrix;
  return true;
}

void DisplayColorManager::OnDisplayConfigurationChanged(
    const display::DisplayConfigurator::DisplayStateList& display_states) {
  size_t displays_with_ctm_support_count = 0;
  for (const display::DisplaySnapshot* state : display_states) {
    if (state->has_color_correction_matrix()) {
      ++displays_with_ctm_support_count;
    }

    const int64_t display_id = state->display_id();
    const auto calibration_iter = calibration_map_.find(state->product_code());
    if (calibration_iter != calibration_map_.end()) {
      DCHECK(calibration_iter->second);
      ApplyDisplayColorCalibration(display_id, *(calibration_iter->second));
    } else if (!LoadCalibrationForDisplay(state)) {
      // Failed to start loading ICC profile. Reset calibration or reapply an
      // existing color matrix we have for this display.
      ResetDisplayColorCalibration(display_id);
    }
  }

  if (!displays_with_ctm_support_count)
    displays_ctm_support_ = DisplayCtmSupport::kNone;
  else if (displays_with_ctm_support_count == display_states.size())
    displays_ctm_support_ = DisplayCtmSupport::kAll;
  else
    displays_ctm_support_ = DisplayCtmSupport::kMixed;
}

void DisplayColorManager::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  for (const auto& display : removed_displays) {
    displays_color_matrix_map_.erase(display.id());
  }
}

void DisplayColorManager::ApplyDisplayColorCalibration(
    int64_t display_id,
    const display::ColorCalibration& calibration) {
  configurator_->SetColorCalibration(display_id, calibration);
}

bool DisplayColorManager::LoadCalibrationForDisplay(
    const display::DisplaySnapshot* display) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (display->display_id() == display::kInvalidDisplayId) {
    LOG(WARNING) << "Trying to load calibration data for invalid display id";
    return false;
  }

  const bool is_valid_product_code =
      display->product_code() != display::DisplaySnapshot::kInvalidProductCode;
  if (!is_valid_product_code || !quirks::QuirksManager::HasInstance()) {
    return false;
  }

  // Look for calibrations for this display. Each calibration may overwrite the
  // previous one.
  // TODO(jchinlee): Consider collapsing queries.
  // TODO(b/290383914): Ensure VPD-written ICC is applied.
  system::StatisticsProvider::GetInstance()->ScheduleOnMachineStatisticsLoaded(
      base::BindOnce(&DisplayColorManager::QueryVpdForCalibration,
                     weak_ptr_factory_.GetWeakPtr(), display->display_id(),
                     display->product_code(),
                     display->has_color_correction_matrix(), display->type()));
  QueryQuirksForCalibration(
      display->display_id(), display->display_name(), display->product_code(),
      display->has_color_correction_matrix(), display->type());
  return true;
}

bool DisplayColorManager::HasVpdDisplayProfilesEntry(
    int64_t product_code) const {
  std::optional<std::string_view> display_profile_string =
      system::StatisticsProvider::GetInstance()->GetMachineStatistic(
          system::kDisplayProfilesKey);
  if (!display_profile_string)
    return false;

  const std::string hex_id = quirks::IdToHexString(product_code);
  if (!display_profile_string->starts_with(hex_id))
    return false;

  return true;
}

void DisplayColorManager::QueryVpdForCalibration(
    int64_t display_id,
    int64_t product_code,
    bool has_color_correction_matrix,
    display::DisplayConnectionType type) {
  if (type != display::DISPLAY_CONNECTION_TYPE_INTERNAL)
    return;

  if (!HasVpdDisplayProfilesEntry(product_code)) {
    return;
  }

  VLOG(1) << "Loading ICC profile for display id: " << display_id
          << " with product id: " << product_code;
  UpdateCalibrationData(display_id, product_code,
                        ParseVpdEntry(has_color_correction_matrix));
}

void DisplayColorManager::QueryQuirksForCalibration(
    int64_t display_id,
    const std::string& display_name,
    int64_t product_code,
    bool has_color_correction_matrix,
    display::DisplayConnectionType type) {
  quirks::QuirksManager::Get()->RequestIccProfilePath(
      product_code, display_name,
      base::BindOnce(&DisplayColorManager::FinishLoadCalibrationForDisplay,
                     weak_ptr_factory_.GetWeakPtr(), display_id, product_code,
                     has_color_correction_matrix, type));
}

void DisplayColorManager::FinishLoadCalibrationForDisplay(
    int64_t display_id,
    int64_t product_code,
    bool has_color_correction_matrix,
    display::DisplayConnectionType type,
    const base::FilePath& path,
    bool file_downloaded) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::string product_string = quirks::IdToHexString(product_code);
  if (path.empty()) {
    VLOG(1) << "No ICC file found with product id: " << product_string
            << " for display id: " << display_id;
    ResetDisplayColorCalibration(display_id);
    return;
  }

  if (file_downloaded && type == display::DISPLAY_CONNECTION_TYPE_INTERNAL) {
    VLOG(1) << "Downloaded ICC file with product id: " << product_string
            << " for internal display id: " << display_id
            << ". Profile will be applied on next startup.";
    ResetDisplayColorCalibration(display_id);
    return;
  }

  VLOG(1) << "Loading ICC file " << path.value()
          << " for display id: " << display_id
          << " with product id: " << product_string;

  sequenced_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ParseDisplayProfileFile, path,
                     has_color_correction_matrix),
      base::BindOnce(&DisplayColorManager::UpdateCalibrationData,
                     weak_ptr_factory_.GetWeakPtr(), display_id, product_code));
}

void DisplayColorManager::UpdateCalibrationData(
    int64_t display_id,
    int64_t product_id,
    std::unique_ptr<display::ColorCalibration> data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Apply the received |data| if valid or reset color calibration.
  if (data) {
    ApplyDisplayColorCalibration(display_id, *data);
    calibration_map_[product_id] = std::move(data);
  } else {
    ResetDisplayColorCalibration(display_id);
  }
}

void DisplayColorManager::ResetDisplayColorCalibration(int64_t display_id) {
  // We must call this in every potential failure point at loading the ICC
  // profile of the displays when the displays have been reconfigured. This is
  // due to the following reason:
  // With the DRM drivers on ChromeOS, the color management tables and matrices
  // are stored at the pipe level (part of the display hardware that is
  // configurable regardless of the actual connector it is attached to). This
  // allows display configuration to remain active while different processes are
  // using the driver (for example switching VT).
  //
  // As a result, when an external screen is connected to a Chromebook, a given
  // color configuration might be applied to it and remain stored in the driver
  // after the screen is disconnected. If another external screen is now
  // connected the previously applied color management will remain if there is
  // not a profile for that display.
  //
  // For more details, please refer to https://crrev.com/1914343003.
  ApplyDisplayColorCalibration(display_id, {} /* calibration_data */);
}

}  // namespace ash
