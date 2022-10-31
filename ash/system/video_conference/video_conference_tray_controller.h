// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_

namespace ash {

// Controller that will act as a "bridge" between VC apps management and the VC
// UI layers. The singleton instance is constructed immediately before and
// destructed immediately after the UI, so any code that keeps a reference to
// it must be prepared to accommodate this specific lifetime in order to prevent
// any use-after-free bugs.
class VideoConferenceTrayController {
 public:
  VideoConferenceTrayController();

  VideoConferenceTrayController(const VideoConferenceTrayController&) = delete;
  VideoConferenceTrayController& operator=(
      const VideoConferenceTrayController&) = delete;

  ~VideoConferenceTrayController();
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_VIDEO_CONFERENCE_TRAY_CONTROLLER_H_