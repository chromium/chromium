// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_GAME_CAPTURE_BAR_VIEW_H_
#define ASH_CAPTURE_MODE_GAME_CAPTURE_BAR_VIEW_H_

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_bar_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class PillButton;

// A view that acts as the content view of the capture mode bar widget of a
// game capture session. The bar only includes a start recording button, the
// settings and close buttons.
class ASH_EXPORT GameCaptureBarView : public CaptureModeBarView {
  METADATA_HEADER(GameCaptureBarView, CaptureModeBarView)

 public:
  GameCaptureBarView();
  GameCaptureBarView(const GameCaptureBarView&) = delete;
  GameCaptureBarView& operator=(const GameCaptureBarView&) = delete;
  ~GameCaptureBarView() override;

  // CaptureModeBarView:
  PillButton* GetStartRecordingButton() const override;

 private:
  // Called when clicking on the start recording button inside the bar.
  void StartRecording();

  raw_ptr<PillButton> start_recording_button_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_GAME_CAPTURE_BAR_VIEW_H_
