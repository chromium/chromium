// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
#define ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_

#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

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
  METADATA_HEADER(StopRecordingButtonTray, TrayBackgroundView)

 public:
  explicit StopRecordingButtonTray(Shelf* shelf);
  StopRecordingButtonTray(const StopRecordingButtonTray&) = delete;
  StopRecordingButtonTray& operator=(const StopRecordingButtonTray&) = delete;
  ~StopRecordingButtonTray() override;

 private:
  // TrayBackgroundView:
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override {}
  // No need to override since this view doesn't have an active/inactive state
  // Clicking on it will stop the recording and make this view disappear.
  void UpdateTrayItemColor(bool is_active) override {}
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override {}
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void OnThemeChanged() override;
  void HideBubble(const TrayBubbleView* bubble_view) override {}

  // Image view of the stop recording icon.
  const raw_ptr<views::ImageView> image_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
