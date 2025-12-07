// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/sunfish_capture_bar_view.h"

#include "ash/strings/grit/ash_strings.h"

namespace ash {

SunfishCaptureBarView::SunfishCaptureBarView() {
  AppendCloseButton(
      IDS_ASH_SCREEN_CAPTURE_SUNFISH_CLOSE_BUTTON_ACCESSIBLE_NAME);
}

SunfishCaptureBarView::~SunfishCaptureBarView() = default;

void SunfishCaptureBarView::SetSettingsMenuShown(bool shown) {}

BEGIN_METADATA(SunfishCaptureBarView)
END_METADATA

}  // namespace ash
