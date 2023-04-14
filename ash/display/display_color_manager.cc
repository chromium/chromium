// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_color_manager.h"

#include <utility>

#include "ash/constants/ash_paths.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "components/quirks/quirks_manager.h"
#include "third_party/qcms/src/qcms.h"
#include "ui/display/display.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/skia_color_space_util.h"

namespace ash {

namespace {

// Runs on a background thread because it does file IO.
std::unique_ptr<DisplayColorManager::ColorCalibrationData> ParseDisplayProfile(
    const base::FilePath& path,
    bool has_color_correction_matrix) {
  VLOG(1) << "Trying ICC file " << path.value()
          << " has_color_correction_matrix: "
          << (has_color_correction_matrix ? "true" : "false");
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  // Reads from a file.
  qcms_profile* display_profile = qcms_profile_from_path(path.value().c_str());
  if (!display_profile) {
    LOG(WARNING) << "Unable to load ICC file: " << path.value();
    return nullptr;
  }

  size_t vcgt_channel_length =
      qcms_profile_get_vcgt_channel_length(display_profile);

  if (!has_color_correction_matrix && !vcgt_channel_length) {
    LOG(WARNING) << "No vcgt table or color correction matrix in ICC file: "
                 << path.value();
    qcms_profile_release(display_profile);
    return nullptr;
  }

  std::unique_ptr<DisplayColorManager::ColorCalibrationData> data(
      new DisplayColorManager::ColorCalibrationData());
  if (vcgt_channel_length) {
    VLOG_IF(1, has_color_correction_matrix)
        << "Using VCGT data on CTM enabled platform.";

    std::vector<uint16_t> vcgt_data;
    vcgt_data.resize(vcgt_channel_length * 3);
    if (!qcms_profile_get_vcgt_rgb_channels(display_profile, &vcgt_data[0])) {
      LOG(WARNING) << "Unable to get vcgt data";
      qcms_profile_release(display_profile);
      return nullptr;
    }

    data->gamma_lut.resize(vcgt_channel_length);
    for (size_t i = 0; i < vcgt_channel_length; ++i) {
      data->gamma_lut[i].r = vcgt_data[i];
      data->gamma_lut[i].g = vcgt_data[vcgt_channel_length + i];
      data->gamma_lut[i].b = vcgt_data[(vcgt_channel_length * 2) + i];
    }
  } else {
    VLOG(1) << "Using full degamma/gamma/CTM from profile.";
    qcms_profile* srgb_profile = qcms_profile_sRGB();

    qcms_transform* transform =
        qcms_transform_create(srgb_profile, QCMS_DATA_RGB_8, display_profile,
                              QCMS_DATA_RGB_8, QCMS_INTENT_PERCEPTUAL);

    if (!transform) {
      LOG(WARNING)
          << "Unable to create transformation from sRGB to display profile.";

      qcms_profile_release(display_profile);
      qcms_profile_release(srgb_profile);
      return nullptr;
    }

    if (!qcms_transform_is_matrix(transform)) {
      LOG(WARNING) << "No transformation matrix available";

      qcms_transform_release(transform);
      qcms_profile_release(display_profile);
      qcms_profile_release(srgb_profile);
      return nullptr;
    }

    size_t degamma_size = qcms_transform_get_input_trc_rgba(
        transform, srgb_profile, QCMS_TRC_USHORT, nullptr);
    size_t gamma_size = qcms_transform_get_output_trc_rgba(
        transform, display_profile, QCMS_TRC_USHORT, nullptr);

    if (degamma_size == 0 || gamma_size == 0) {
      LOG(WARNING)
          << "Invalid number of elements in gamma tables: degamma size = "
          << degamma_size << " gamma size = " << gamma_size;

      qcms_transform_release(transform);
      qcms_profile_release(display_profile);
      qcms_profile_release(srgb_profile);
      return nullptr;
    }

    std::vector<uint16_t> degamma_data;
    std::vector<uint16_t> gamma_data;
    degamma_data.resize(degamma_size * 4);
    gamma_data.resize(gamma_size * 4);

    qcms_transform_get_input_trc_rgba(transform, srgb_profile, QCMS_TRC_USHORT,
                                      &degamma_data[0]);
    qcms_transform_get_output_trc_rgba(transform, display_profile,
                                       QCMS_TRC_USHORT, &gamma_data[0]);

    data->degamma_lut.resize(degamma_size);
    for (size_t i = 0; i < degamma_size; ++i) {
      data->degamma_lut[i].r = degamma_data[i * 4];
      data->degamma_lut[i].g = degamma_data[(i * 4) + 1];
      data->degamma_lut[i].b = degamma_data[(i * 4) + 2];
    }

    data->gamma_lut.resize(gamma_size);
    for (size_t i = 0; i < gamma_size; ++i) {
      data->gamma_lut[i].r = gamma_data[i * 4];
      data->gamma_lut[i].g = gamma_data[(i * 4) + 1];
      data->gamma_lut[i].b = gamma_data[(i * 4) + 2];
    }

    data->correction_matrix.resize(9);
    for (int i = 0; i < 9; ++i) {
      data->correction_matrix[i] =
          qcms_transform_get_matrix(transform, i / 3, i % 3);
    }

    qcms_transform_release(transform);
    qcms_profile_release(srgb_profile);
  }

  VLOG(1) << "ICC file successfully parsed";
  qcms_profile_release(display_profile);
  return data;
}

// Fills |out_result_matrix_vector| from the given skia |matrix|.
void ColorMatrixVectorFromSkMatrix44(
    const SkM44& matrix,
    std::vector<float>* out_result_matrix_vector) {
  DCHECK(out_result_matrix_vector);
  out_result_matrix_vector->assign(9, 0.0f);
  (*out_result_matrix_vector)[0] = matrix.rc(0, 0);
  (*out_result_matrix_vector)[4] = matrix.rc(1, 1);
  (*out_result_matrix_vector)[8] = matrix.rc(2, 2);
}

SkM44 SkMatrix44FromColorMatrixVector(const std::vector<float>& matrix_vector) {
  if (matrix_vector.empty())
    return SkM44();

  DCHECK_EQ(matrix_vector.size(), 9u);
  return gfx::SkM44FromRowMajor3x3(matrix_vector.data());
}

bool HasColorCorrectionMatrix(display::DisplayConfigurator* configurator,
                              int64_t display_id) {
  for (const auto* display_snapshot : configurator->cached_displays()) {
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

bool DisplayColorManager::SetDisplayColorMatrix(int64_t display_id,
                                                const SkM44& color_matrix) {
  for (const auto* display_snapshot : configurator_->cached_displays()) {
    if (display_snapshot->display_id() != display_id)
      continue;

    return SetDisplayColorMatrix(display_snapshot, color_matrix);
  }

  LOG(ERROR) << "Display ID: " << display_id << " cannot be found.";
  return false;
}

bool DisplayColorManager::SetDisplayColorMatrix(
    const display::DisplaySnapshot* display_snapshot,
    const SkM44& color_matrix) {
  DCHECK(display_snapshot);
  DCHECK(base::Contains(configurator_->cached_displays(), display_snapshot));

  if (!display_snapshot->has_color_correction_matrix()) {
    // This display doesn't support setting a CRTC matrix.
    return false;
  }

  // Always overwrite any existing matrix for this display.
  const int64_t display_id = display_snapshot->display_id();
  displays_color_matrix_map_[display_id] = color_matrix;
  const auto iter = calibration_map_.find(display_snapshot->product_code());
  SkM44 combined_matrix = color_matrix;
  if (iter != calibration_map_.end()) {
    DCHECK(iter->second);
    combined_matrix.preConcat(
        SkMatrix44FromColorMatrixVector(iter->second->correction_matrix));
  }

  ColorMatrixVectorFromSkMatrix44(combined_matrix, &matrix_buffer_);
  return configurator_->SetColorMatrix(display_id, matrix_buffer_);
}

void DisplayColorManager::OnDisplayModeChanged(
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

void DisplayColorManager::OnDisplayRemoved(
    const display::Display& old_display) {
  displays_color_matrix_map_.erase(old_display.id());
}

void DisplayColorManager::ApplyDisplayColorCalibration(
    int64_t display_id,
    const ColorCalibrationData& calibration_data) {
  if (HasColorCorrectionMatrix(configurator_, display_id)) {
    const auto color_matrix_iter = displays_color_matrix_map_.find(display_id);
    const std::vector<float>* final_matrix =
        &calibration_data.correction_matrix;
    if (color_matrix_iter != displays_color_matrix_map_.end()) {
      SkM44 combined_matrix = color_matrix_iter->second;
      combined_matrix.preConcat(SkMatrix44FromColorMatrixVector(*final_matrix));
      ColorMatrixVectorFromSkMatrix44(combined_matrix, &matrix_buffer_);
      final_matrix = &matrix_buffer_;
    }

    if (!configurator_->SetColorMatrix(display_id, *final_matrix))
      LOG(WARNING) << "Error applying the color matrix.";
  }

  if (!configurator_->SetGammaCorrection(display_id,
                                         calibration_data.degamma_lut,
                                         calibration_data.gamma_lut)) {
    LOG(WARNING) << "Error applying gamma correction data.";
  }
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
  QueryVpdForCalibration(display->display_id(), display->product_code(),
                         display->has_color_correction_matrix(),
                         display->type());
  QueryQuirksForCalibration(
      display->display_id(), display->display_name(), display->product_code(),
      display->has_color_correction_matrix(), display->type());
  return true;
}

void DisplayColorManager::QueryVpdForCalibration(
    int64_t display_id,
    int64_t product_code,
    bool has_color_correction_matrix,
    display::DisplayConnectionType type) {
  if (type != display::DISPLAY_CONNECTION_TYPE_INTERNAL)
    return;

  base::FilePath directory;
  base::PathService::Get(DIR_DEVICE_DISPLAY_PROFILES_VPD, &directory);
  const std::string icc_name = quirks::IdToFileName(product_code);
  const base::FilePath icc_path = directory.Append(icc_name);

  sequenced_task_runner_.get()->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&base::PathExists, icc_path),
      base::BindOnce(&DisplayColorManager::FinishQueryVpdForCalibration,
                     weak_ptr_factory_.GetWeakPtr(), display_id, product_code,
                     has_color_correction_matrix, type, icc_path));
}

void DisplayColorManager::FinishQueryVpdForCalibration(
    int64_t display_id,
    int64_t product_code,
    bool has_color_correction_matrix,
    display::DisplayConnectionType type,
    const base::FilePath& expected_icc_path,
    bool found_icc) {
  if (!found_icc)
    return;

  DisplayColorManager::FinishLoadCalibrationForDisplay(
      display_id, product_code, has_color_correction_matrix, type,
      expected_icc_path, false);
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
      base::BindOnce(&ParseDisplayProfile, path, has_color_correction_matrix),
      base::BindOnce(&DisplayColorManager::UpdateCalibrationData,
                     weak_ptr_factory_.GetWeakPtr(), display_id, product_code));
}

void DisplayColorManager::UpdateCalibrationData(
    int64_t display_id,
    int64_t product_id,
    std::unique_ptr<ColorCalibrationData> data) {
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

DisplayColorManager::ColorCalibrationData::ColorCalibrationData()
    : correction_matrix{1, 0, 0, 0, 1, 0, 0, 0, 1} {}

DisplayColorManager::ColorCalibrationData::~ColorCalibrationData() = default;

}  // namespace ash
