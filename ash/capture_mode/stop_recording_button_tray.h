// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
#define ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_

#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"

namespace views {
class ImageView;
}

namespace ash {

class Shelf;
class TrayBubbleView;

// Status area tray which is visible when a video is being recorded using
// capture mode. Tapping this tray will stop recording. This tray does not
// provide any bubble view windows.
class StopRecordingButtonTray : public TrayBackgroundView {
 public:
  explicit StopRecordingButtonTray(Shelf* shelf);
  StopRecordingButtonTray(const StopRecordingButtonTray&) = delete;
  StopRecordingButtonTray& operator=(const StopRecordingButtonTray&) = delete;
  ~StopRecordingButtonTray() override;

 private:
  // TrayBackgroundView:
  void ClickedOutsideBubble() override {}
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void OnThemeChanged() override;

  // Image view of the stop recording icon.
  const raw_ptr<views::ImageView, ExperimentalAsh> image_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
