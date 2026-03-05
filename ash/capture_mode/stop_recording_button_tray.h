// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
#define ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_

#include "ash/system/tray/imaged_tray_icon.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash {

class Shelf;

// Status area tray which is visible when a video is being recorded using
// capture mode. Tapping this tray will stop recording. This tray does not
// provide any bubble view windows.
class StopRecordingButtonTray : public ImagedTrayIcon {
  METADATA_HEADER(StopRecordingButtonTray, ImagedTrayIcon)

 public:
  explicit StopRecordingButtonTray(Shelf* shelf);
  StopRecordingButtonTray(const StopRecordingButtonTray&) = delete;
  StopRecordingButtonTray& operator=(const StopRecordingButtonTray&) = delete;
  ~StopRecordingButtonTray() override;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_STOP_RECORDING_BUTTON_TRAY_H_
