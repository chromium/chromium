// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_VIDEO_VIEW_H_
#define ASH_AMBIENT_UI_AMBIENT_VIDEO_VIEW_H_

#include <memory>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/constants/ambient_video.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class AmbientSlideshowPeripheralUi;
class AmbientViewDelegate;
class AshWebView;

// Plays a video on loop. The entire contents of the view are filled with the
// rendered video. Internally, this is implemented by rendering a simple HTML
// page with a <video> element in it.
class ASH_EXPORT AmbientVideoView : public views::View {
 public:
  // |video_file|: Name of video file to play.
  // |html_path|: Path of the HTML source file with the <video> element in it.
  //              This is loaded by constructing a "file://" URL pointing to
  //              this HTML file. The |video_file| is passed to the HTML via
  //              a query parameter in the URL like so:
  //              file://<html_path>?video_src=<video_file>
  //
  // Important Note:
  // The parent directory for |html_path| and the directory of the video itself
  // (currently hard-coded in the HTML file) must be present in the allowlist in
  // chrome/browser/net/chrome_network_delegate.cc, or the webpage will fail to
  // load.
  AmbientVideoView(std::string_view video_file,
                   const base::FilePath& html_path,
                   AmbientVideo video,
                   AmbientViewDelegate* view_delegate);
  AmbientVideoView(const AmbientVideoView&) = delete;
  AmbientVideoView& operator=(const AmbientVideoView&) = delete;
  ~AmbientVideoView() override;

 private:
  const AmbientVideo video_;
  raw_ptr<AshWebView> ash_web_view_ = nullptr;
  // Per UX: Uses the exact same spec for peripheral UI elements (weather, time,
  // etc) as the slideshow theme.
  const std::unique_ptr<AmbientSlideshowPeripheralUi> peripheral_ui_;
  base::RepeatingTimer peripheral_ui_jitter_timer_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_VIDEO_VIEW_H_
