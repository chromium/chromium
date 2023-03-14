// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/ambient_video_utils.h"

#include "ash/public/cpp/personalization_app/time_of_day_paths.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"

namespace ash {
namespace {

// For development/debugging purposes only. Developers may want to quickly test
// with their own version of the video in a different directory.
constexpr base::StringPiece kAmbientVideoDirSwitch = "ambient-video-dir";

base::FilePath GetVideoDir() {
  base::FilePath ambient_video_dir =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          kAmbientVideoDirSwitch);
  return ambient_video_dir.empty()
             ? personalization_app::GetTimeOfDayVideosDir()
             : ambient_video_dir;
}

}  // namespace

base::FilePath GetAmbientVideoPath(AmbientVideo video) {
  base::StringPiece video_file_name;
  switch (video) {
    case AmbientVideo::kNewMexico:
      video_file_name = personalization_app::kTimeOfDayNewMexicoVideo;
      break;
    case AmbientVideo::kClouds:
      video_file_name = personalization_app::kTimeOfDayCloudsVideo;
      break;
  }
  // TODO(b/271182121): Check that the ambient video actually exists on disc and
  // record UMA metric if it does not. In the current design, these videos are
  // stored on rootfs, so this should never happen.
  return GetVideoDir().Append(video_file_name);
}

}  // namespace ash
