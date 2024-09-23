// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/color_enhancement/color_enhancement_controller.h"

#include <memory>

#include "ash/shell.h"
#include "cc/paint/filter_operation.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/matrix3_f.h"

namespace ash {

namespace {

//
// Parameters for simulating color vision changes.
// Copied from the Javascript ColorEnhancer extension:
//   ui/accessibility/extensions/colorenhancer/src/cvd.js
// Initial source:
//   http://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/CVD_Simulation.html
// Original Research Paper:
//   http://www.inf.ufrgs.br/~oliveira/pubs_files/CVD_Simulation/Machado_Oliveira_Fernandes_CVD_Vis2009_final.pdf
//
// The first index is ColorVisionCorrectionType enum, so this must be kept in
// that order.
const float kSimulationParams[3][9][3] = {
    // ColorVisionCorrectionType::kProtanomaly:
    {{0.4720, -1.2946, 0.9857},
     {-0.6128, 1.6326, 0.0187},
     {0.1407, -0.3380, -0.0044},
     {-0.1420, 0.2488, 0.0044},
     {0.1872, -0.3908, 0.9942},
     {-0.0451, 0.1420, 0.0013},
     {0.0222, -0.0253, -0.0004},
     {-0.0290, -0.0201, 0.0006},
     {0.0068, 0.0454, 0.9990}},
    // ColorVisionCorrectionType::kDeuteranomaly:
    {{0.5442, -1.1454, 0.9818},
     {-0.7091, 1.5287, 0.0238},
     {0.1650, -0.3833, -0.0055},
     {-0.1664, 0.4368, 0.0056},
     {0.2178, -0.5327, 0.9927},
     {-0.0514, 0.0958, 0.0017},
     {0.0180, -0.0288, -0.0006},
     {-0.0232, -0.0649, 0.0007},
     {0.0052, 0.0360, 0.9998}},
    // ColorVisionCorrectionType::kTritanomaly:
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
gfx::Matrix3F GetCvdSimulationMatrix(ColorVisionCorrectionType type,
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
// to shift original colors to be more visible for a simulation with the given
// `type` and `severity`.
gfx::Matrix3F ComputeColorVisionFilterMatrix(ColorVisionCorrectionType type,
                                             float severity) {
  // Compute the matrix that could be used to simulate the color vision.
  gfx::Matrix3F simulation_matrix = GetCvdSimulationMatrix(type, severity);

  // Now use the simulation to calculate a correction matrix. This process is
  // called Daltonizing.

  // "Daltonizing" an image consists of calculating the error, which is the
  // original image minus the simulation and represents the information lost to
  // the user, and linearly transforming that error to a color space the user
  // can see, then adding it back onto the original image (Fidaner, Lin and
  // Ozguven, 2006). The correction matrix is used to map the error between the
  // initial image and the simulated image into a color space that can be seen
  // by the user based on the type of color deficiency. So for example someone
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
    case ColorVisionCorrectionType::kProtanomaly:
      // Correct on red axis: Shift colors in the red channel to the other
      // channels.
      correction_matrix.set(0.0, 0.0, 0.0, 0.7, 1.0, 0.0, 0.7, 0.0, 1.0);
      break;
    case ColorVisionCorrectionType::kDeuteranomaly:
      // Correct on green axis: Shift colors in the green channel to the other
      // channels.
      correction_matrix.set(1.0, 0.7, 0.0, 0.0, 0.0, 0.0, 0.0, 0.7, 1.0);
      break;
    case ColorVisionCorrectionType::kTritanomaly:
      // Correct on blue axis: Shift colors in the blue channel into the other
      // channels.
      correction_matrix.set(1.0, 0.0, 0.7, 0.0, 1.0, 0.7, 0.0, 0.0, 0.0);
      break;
    case ash::ColorVisionCorrectionType::kGrayscale:
      NOTREACHED() << "Grayscale should be handled in SetGreyscaleAmount";
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

void UpdateNotificationFlashMatrix(const SkColor& original_color,
                                   cc::FilterOperation::Matrix* matrix) {
  SkScalar hsv[3];
  SkColorToHSV(original_color, hsv);

  // We use 30% of the original color, similar to Android's
  // packages/apps/Settings/res/values/colors.xml.
  hsv[1] *= 0.3;

  const SkColor color = SkHSVToColor(hsv);
  const float r = SkColorGetR(color);
  const float g = SkColorGetG(color);
  const float b = SkColorGetB(color);
  // `matrix` represents a 5x4 matrix where the top 4x4 matrix is
  // r, g, b and alpha. If we were not mutating the color, this 4x4
  // should be the identity. When adding a tint, set r, g and b
  // based on the desired tint color.
  (*matrix)[0] = r / 255.0;
  (*matrix)[6] = g / 255.0;
  (*matrix)[12] = b / 255.0;
}

}  // namespace

ColorEnhancementController::ColorEnhancementController() {
  Shell::Get()->AddShellObserver(this);

  // Initialize the notification flash matrix with zeros.
  notification_flash_matrix_ = std::make_unique<cc::FilterOperation::Matrix>();
  for (int i = 0; i < 19; i++) {
    (*notification_flash_matrix_)[i] = 0;
  }

  // `notification_flash_matrix_` represents a 5x4 matrix where the top 4x4
  // matrix is r, g, b and alpha. Use the identity to keep color the same;
  // update r, g and b dynamically when the tint changes.
  (*notification_flash_matrix_)[0] = (*notification_flash_matrix_)[6] =
      (*notification_flash_matrix_)[12] = (*notification_flash_matrix_)[18] = 1;
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

void ColorEnhancementController::SetColorCorrectionEnabledAndUpdateDisplays(
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

void ColorEnhancementController::SetColorVisionCorrectionFilter(
    ColorVisionCorrectionType type,
    float amount) {
  if (type == ColorVisionCorrectionType::kGrayscale) {
    SetGreyscaleAmount(amount);
    cvd_correction_matrix_.reset();
    return;
  }

  SetGreyscaleAmount(0);
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

void ColorEnhancementController::FlashScreenForNotification(
    bool show_flash,
    const SkColor& color) {
  if (!show_flash) {
    UpdateAllDisplays();
    return;
  }

  UpdateNotificationFlashMatrix(color, notification_flash_matrix_.get());
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    ui::Layer* layer = root_window->layer();
    layer->SetLayerCustomColorMatrix(*notification_flash_matrix_);
  }
}

void ColorEnhancementController::OnRootWindowAdded(aura::Window* root_window) {
  UpdateDisplay(root_window);
}

void ColorEnhancementController::UpdateAllDisplays() {
  for (aura::Window* root_window : Shell::GetAllRootWindows()) {
    UpdateDisplay(root_window);
  }
}

void ColorEnhancementController::UpdateDisplay(aura::Window* root_window) {
  ui::Layer* layer = root_window->layer();
  layer->SetLayerInverted(high_contrast_enabled_);

  if (!color_filtering_enabled_) {
    // Reset layer state to defaults.
    layer->SetLayerGrayscale(0.0);
    layer->ClearLayerCustomColorMatrix();
    return;
  }

  layer->SetLayerGrayscale(greyscale_amount_);
  if (cvd_correction_matrix_) {
    layer->SetLayerCustomColorMatrix(*cvd_correction_matrix_);
  } else {
    layer->ClearLayerCustomColorMatrix();
  }
}

}  // namespace ash
