// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_RECORDING_OVERLAY_VIEW_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_RECORDING_OVERLAY_VIEW_H_

#include "ash/public/cpp/ash_public_export.h"
#include "ui/views/view.h"

namespace ash {

// Defines a base view that will be used as the content view of the recording
// overlay widget, which is added as a child window of the surface being
// recorded and laid on top of it, so its contents show up in the recording.
// It's defined here since Ash cannot depend directly on `content/` and this
// view can host a |views::WebView| and its associated |WebContents|, to show
// things such as ink annotations.
class ASH_PUBLIC_EXPORT RecordingOverlayView : public views::View {
 public:
  ~RecordingOverlayView() override = default;

  // TODO(afakhry): Add new APIs here as needed.

 protected:
  RecordingOverlayView() = default;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_RECORDING_OVERLAY_VIEW_H_
