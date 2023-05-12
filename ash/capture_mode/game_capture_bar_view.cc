// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/game_capture_bar_view.h"

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/pill_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace ash {

GameCaptureBarView::GameCaptureBarView()
    : start_recording_button_(AddChildView(std::make_unique<PillButton>(
          base::BindRepeating(&GameCaptureBarView::StartRecording,
                              base::Unretained(this)),
          l10n_util::GetStringUTF16(
              IDS_ASH_GAME_CAPTURE_START_RECORDING_BUTTON),
          PillButton::kPrimaryWithoutIcon))) {
  AppendCommonElements();
}

GameCaptureBarView::~GameCaptureBarView() = default;

PillButton* GameCaptureBarView::GetStartRecordingButton() const {
  return start_recording_button_;
}

void GameCaptureBarView::StartRecording() {
  // TODO(b/282193685): Start recording once the selected window is set and not
  // changeable after game capture session starts.
}

BEGIN_METADATA(GameCaptureBarView, CaptureModeBarView)
END_METADATA

}  // namespace ash
