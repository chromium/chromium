// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_LOGO_VIEW_SHAPE_MIC_PART_SHAPE_H_
#define ASH_ASSISTANT_UI_LOGO_VIEW_SHAPE_MIC_PART_SHAPE_H_

#include <memory>

#include "ash/assistant/ui/logo_view/shape/shape.h"
#include "chromeos/assistant/internal/logo_view/logo_view_constants.h"

namespace chromeos {
namespace assistant {
class Mic;
}  // namespace assistant
}  // namespace chromeos

namespace ash {

// Creates a Path that can morph into a Mic part.
class MicPartShape : public Shape {
 public:
  explicit MicPartShape(float dot_size);

  MicPartShape(const MicPartShape&) = delete;
  MicPartShape& operator=(const MicPartShape&) = delete;

  ~MicPartShape() override;

  // Calculate the |first_path_|, |first_stroke_width_|, and |cap_| based on
  // |progress|, which is the progress when a dot morphs into a Mic part.
  void ToMicPart(float progress, chromeos::assistant::DotColor dot_color);

 private:
  std::unique_ptr<chromeos::assistant::Mic> mic_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_LOGO_VIEW_SHAPE_MIC_PART_SHAPE_H_
