// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_MIC_INDICATOR_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_MIC_INDICATOR_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace base {
class RepeatingTimer;
}  // namespace base

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ash::video_conference {

// Part of the vc title header that shows a wave that indicates
// how loud is the audio captured by the microphone.
class MicIndicator : public views::BoxLayoutView {
  METADATA_HEADER(MicIndicator, views::BoxLayoutView)
 public:
  explicit MicIndicator();
  MicIndicator(const MicIndicator&) = delete;
  MicIndicator& operator=(const MicIndicator&) = delete;
  ~MicIndicator() override;

  void OnPaint(gfx::Canvas* canvas) override;

 private:
  void UpdateProgress();

  float power_;
  ui::ColorId color_;
  int step_;
  // Timer to continuously update the wave when the microphone is capturing
  // audio.
  std::unique_ptr<base::RepeatingTimer> timer_;
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_MIC_INDICATOR_H_
