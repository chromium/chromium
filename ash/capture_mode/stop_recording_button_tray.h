// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
#define ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_

#include "ash/system/tray/tray_background_view.h"

namespace ash {

// Status area tray which is visible when a video is being recorded using
// capture mode. Tapping this tray will stop recording. This tray does not
// provide any bubble view windows.
class StopRecordingButtonTray : public TrayBackgroundView {
 public:
  explicit StopRecordingButtonTray(Shelf* shelf);
  StopRecordingButtonTray(const StopRecordingButtonTray&) = delete;
  StopRecordingButtonTray& operator=(const StopRecordingButtonTray&) = delete;
  ~StopRecordingButtonTray() override;

  // TrayBackgroundView:
  bool PerformAction(const ui::Event& event) override;
  void ClickedOutsideBubble() override {}
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void OnThemeChanged() override;

  // Image view of the stop recording icon.
  views::ImageView* const image_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
