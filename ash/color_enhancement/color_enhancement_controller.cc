// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/color_enhancement/color_enhancement_controller.h"
#include <memory>

#include "ash/shell.h"
#include "cc/paint/filter_operation.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/matrix3_f.h"

namespace ash {

namespace {

// Sepia filter above .3 should enable cursor compositing. Beyond this point,
// users can perceive the mouse is too white if compositing does not occur.
// TODO (crbug.com/1031959): Check this value with UX to see if it can be
// larger.
const float kMinSepiaPerceptableDifference = 0.3f;

//
// Parameters for simulating color vision deficiency.
// Copied from the Javascript ColorEnhancer extension:
//   ui/accessibility/extensions/colorenhancer/src/cvd.js
// Initial source:
//   http://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/CVD_Simulation.html
// Original Research Paper:
//   http://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/Machado_Oliveira_Fernandes_CVD_Vis2009_final.pdf
//
// The first index is ColorVisionDeficiencyType enum, so this must be kept in
// that order.
const float kSimulationParams[3][9][3] = {
    // ColorVisionDeficiencyType::kProtanomaly:
    {{0.4720, -1.2946, 0.9857},
     {-0.6128, 1.6326, 0.0187},
     {0.1407, -0.3380, -0.0044},
     {-0.1420, 0.2488, 0.0044},
     {0.1872, -0.3908, 0.9942},
     {-0.0451, 0.1420, 0.0013},
     {0.0222, -0.0253, -0.0004},
     {-0.0290, -0.0201, 0.0006},
     {0.0068, 0.0454, 0.9990}},
    // ColorVisionDeficiencyType::kDeuteranomaly:
    {{0.5442, -1.1454, 0.9818},
     {-0.7091, 1.5287, 0.0238},
     {0.1650, -0.3833, -0.0055},
     {-0.1664, 0.4368, 0.0056},
     {0.2178, -0.5327, 0.9927},
     {-0.0514, 0.0958, 0.0017},
     {0.0180, -0.0288, -0.0006},
     {-0.0232, -0.0649, 0.0007},
     {0.0052, 0.0360, 0.9998}},
    // ColorVisionDeficiencyType::kTritanomaly:
    {{0.4275, -0.0181, 0.9307},
     {-0.2454, 0.0013, 0.0827},
     {-0.1821, 0.0168, -0.0134},
     {-0.1280, 0.0047, 0.0202},
     {0.0233, -0.0398, 0.9728},
     {0.1048, 0.0352, 0.0070},
     {-0.0156, 0.0061, 0.0071},
     {0.3841, 0.2947, 0.0151},
     {-0.3685, -0.3008, 0.9778}}};

// Returns a 3x3 matrix for simulating the given type of CVD with the given
// severity.
// Calculation from CVD.getCvdSimulationMatrix_ in
// ui/accessibility/extensions/colorenhancer/src/cvd.js.
gfx::Matrix3F GetCvdSimulationMatrix(ColorVisionDeficiencyType type,
                                     float severity) {
  float severity_squared = severity * severity;
  gfx::Matrix3F result = gfx::Matrix3F::Zeros();
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      int param_row = i * 3 + j;
      result.set(i, j,
                 kSimulationParams[type][param_row][0] * severity_squared +
                     kSimulationParams[type][param_row][1] * severity +
                     kSimulationParams[type][param_row][2]);
    }
  }
  return result;
}

// Computes a 3x3 matrix that can be applied to any three-color-channel image
// to shift original colors to be more visible for someone with the given `type`
// and `severity` of color vision deficiency.
gfx::Matrix3F ComputeColorVisionFilterMatrix(ColorVisionDeficiencyType type,
                                             float severity) {
  // Compute the matrix that could be used to simulate the color vision
  // deficiency.
  gfx::Matrix3F simulation_matrix = GetCvdSimulationMatrix(type, severity);

  // Now use the simulation to calculate a correction matrix. This process is
  // called Daltonizing.

  // "Daltonizing" an image consists of calculating the error, which is the
  // original image minus the simulation and represents the information lost to
  // the user, and linearly transforming that error to a color space the user
  // can see, then adding it back onto the original image (Fidaner, Lin and
  // Ozguven, 2006). The correction matrix is used to map the error between the
  // initial image and the simulated image into a color space that can be seen
  // by the user based on their type of color deficiency. So for example someone
  // with Protanopia can see less of the red channel, so the correction matrix
  // could be:
  //    [0.0, 0.0, 0.0,
  //     0.7, 1.0, 0.0,
  //     0.7, 0.0, 1.0]
  // Multiplying this correction matrix by the error and adding it back to the
  // original will have the effect of shifting more of image information lost to
  // protanopes back into the image in a part of the spectrum they can see.
  // Similarly, for Deuteranopia we correct on the green axis, and for
  // Tritanopia we correct on the blue axis.
  gfx::Matrix3F correction_matrix = gfx::Matrix3F::Zeros();
  switch (type) {
    case ColorVisionDeficiencyType::kProtanomaly:
      // Correct on red axis: Shift colors in the red channel to the other
      // channels.
      correction_matrix.set(0.0, 0.0, 0.0, 0.7, 1.0, 0.0, 0.7, 0.0, 1.0);
      break;
    case ColorVisionDeficiencyType::kDeuteranomaly:
      // Correct on green axis: Shift colors in the green channel to the other
      // channels.
      correction_matrix.set(1.0, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.7, 1.0);
      break;
    case ColorVisionDeficiencyType::kTritanomaly:
      // Correct on blue axis: Shift colors in the blue channel into the other
      // channels.
      correction_matrix.set(1.0, 0.0, 0.7, 0.0, 1.0, 0.7, 0.0, 0.0, 0.0);
      break;
  }

  // For Daltonization of an image `original_img`, we would calculate the
  // Daltonized version based on the `simulated_img` image and the
  // `correction_matrix` as:
  //
  //     result_img = original_img + correction_matrix x (original_img -
  //         simulated_img)
  //
  // We know that simulation_matrix x original_img = simulated_img, and can
  // substitute:
  //
  //     result_image = original_img + correction_matrix x (original_img -
  //         simulation_matrix x original_img)
  //
  // We can factor out `original_img` because matrix multiplication distributes:
  //
  //     result_image =  (Identity + correction_matrix x
  //         (Identity - simulation_matrix)) x original_img
  //
  // This method should return the matrix that multiplies by `original_img` to
  // get the result, i.e.
  //
  //     result_matrix = Identity + correction_matrix x (Identity -
  //         simulation_matrix)
  //
  // Distributing the `correction_matrix` gives us:
  //
  //     result_matrix = Identity + correction_matrix - correction_matrix x
  //         simulation_matrix
  //
  // Compute this and return it.
  return gfx::Matrix3F::Identity() + correction_matrix -
         MatrixProduct(correction_matrix, simulation_matrix);
}

}  // namespace

ColorEnhancementController::ColorEnhancementController() {
  Shell::Get()->AddShellObserver(this);
}

ColorEnhancementController::~ColorEnhancementController() {
  Shell::Get()->RemoveShellObserver(this);
}

void ColorEnhancementController::SetHighContrastEnabled(bool enabled) {
  if (high_contrast_enabled_ == enabled)
    return;

  high_contrast_enabled_ = enabled;
  // Enable cursor compositing so the cursor is also inverted.
  Shell::Get()->UpdateCursorCompositingEnabled();
  UpdateAllDisplays();
}

void ColorEnhancementController::SetColorFilteringEnabledAndUpdateDisplays(
    bool enabled) {
  color_filtering_enabled_ = enabled;
  UpdateAllDisplays();
}

void ColorEnhancementController::SetGreyscaleAmount(float amount) {
  if (greyscale_amount_ == amount || amount < 0 || amount > 1)
    return;

  greyscale_amount_ = amount;
  // Note: No need to do cursor compositing since cursors are greyscale already.
}

void ColorEnhancementController::SetSaturationAmount(float amount) {
  if (saturation_amount_ == amount || amount < 0)
    return;

  saturation_amount_ = amount;
  // Note: No need to do cursor compositing since cursors are greyscale and not
  // impacted by saturation.
}

void ColorEnhancementController::SetSepiaAmount(float amount) {
  if (sepia_amount_ == amount || amount < 0 || amount > 1)
    return;

  sepia_amount_ = amount;
  // The cursor should be tinted sepia as well. Update cursor compositing.
  Shell::Get()->UpdateCursorCompositingEnabled();
}

void ColorEnhancementController::SetHueRotationAmount(int amount) {
  if (hue_rotation_amount_ == amount || amount < 0 || amount > 359)
    return;

  hue_rotation_amount_ = amount;
  // Note: No need to do cursor compositing since cursors are greyscale and not
  // impacted by hue rotation.
}

void ColorEnhancementController::SetColorVisionCorrectionFilter(
    ColorVisionDeficiencyType type,
    float amount) {
  if ((amount <= 0 || amount > 1) && cvd_correction_matrix_) {
    cvd_correction_matrix_.reset();
    return;
  }

  gfx::Matrix3F filter_matrix = ComputeColorVisionFilterMatrix(type, amount);

  // The color matrix used by ui::Layer is a 4 x 5 matrix.
  // Convert the 3x3 result into the shape needed for the layer.
  cvd_correction_matrix_ = std::make_unique<cc::FilterOperation::Matrix>();
  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 5; col++) {
      int index = row * 5 + col;
      if (row < 3 && col < 3) {
        (*cvd_correction_matrix_)[index] = filter_matrix.get(row, col);
      } else if (index != 18) {
        (*cvd_correction_matrix_)[index] = 0;
      } else {
        (*cvd_correction_matrix_)[index] = 1;
      }
    }
  }
}

bool ColorEnhancementController::ShouldEnableCursorCompositingForSepia() const {
  if (!::features::
          AreExperimentalAccessibilityColorEnhancementSettingsEnabled()) {
    return false;
  }

  // Enable cursor compositing if the sepia filter is on enough that
  // the white mouse cursor stands out. Sepia will not be set on the root
  // window if the setting value is greater than 1, so ignore that state.
  return sepia_amount_ >= kMinSepiaPerceptableDifference && sepia_amount_ <= 1;
}

void ColorEnhancementController::OnRootWindowAdded(aura::Window* root_window) {
  UpdateDisplay(root_window);
}

void ColorEnhancementController::UpdateAllDisplays() {
  for (auto* root_window : Shell::GetAllRootWindows())
    UpdateDisplay(root_window);
}

void ColorEnhancementController::UpdateDisplay(aura::Window* root_window) {
  ui::Layer* layer = root_window->layer();
  layer->SetLayerInverted(high_contrast_enabled_);

  if (!::features::
          AreExperimentalAccessibilityColorEnhancementSettingsEnabled()) {
    return;
  }

  if (!color_filtering_enabled_) {
    // Reset layer state to defaults.
    layer->SetLayerGrayscale(0.0);
    layer->SetLayerSaturation(1.0);
    layer->SetLayerSepia(0);
    layer->SetLayerHueRotation(0);
    layer->ClearLayerCustomColorMatrix();
    return;
  }

  layer->SetLayerGrayscale(greyscale_amount_);
  layer->SetLayerSaturation(saturation_amount_);
  layer->SetLayerSepia(sepia_amount_);
  layer->SetLayerHueRotation(hue_rotation_amount_);
  if (cvd_correction_matrix_) {
    layer->SetLayerCustomColorMatrix(*cvd_correction_matrix_);
  } else {
    layer->ClearLayerCustomColorMatrix();
  }
}

}  // namespace ash
