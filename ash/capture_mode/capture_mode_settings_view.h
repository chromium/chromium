// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_VIEW_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_VIEW_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {

class CaptureModeBarView;
class CaptureModeSettingsEntryView;

// A view that acts as the content view of the capture mode settings menu
// widget. It has settings entries, starting with a microphone toggle. The
// structure looks like this:
//
//   +-----------------------------------------------------------+
//   |  +-----------------------------------------------------+  |
//   |  |  +---+  +------------------------------+  +------+  |  |
//   |  |  |   |  |                              |  |      |  |  |
//   |  |  +---+  +------------------------------+  +------+  |  |
//   |  +-----------------------------------------------------+  |
//   +--^--------------------------------------------------------+
//   ^  |
//   |  CaptureModeSettingsEntryView
//   |
//   CaptureModeSettingsView
//
class ASH_EXPORT CaptureModeSettingsView : public views::View {
 public:
  METADATA_HEADER(CaptureModeSettingsView);

  // |projector_mode| specifies whether the current capture mode session was
  // started for the projector workflow. In this mode, only a limited set of
  // capture mode settings are exposed to the user.
  explicit CaptureModeSettingsView(bool projector_mode);
  CaptureModeSettingsView(const CaptureModeSettingsView&) = delete;
  CaptureModeSettingsView& operator=(const CaptureModeSettingsView&) = delete;
  ~CaptureModeSettingsView() override;

  CaptureModeSettingsEntryView* microphone_view() const {
    return microphone_view_;
  }

  // Gets the ideal bounds in screen coordinates of the settings widget on
  // the given |capture_mode_bar_view|.
  static gfx::Rect GetBounds(CaptureModeBarView* capture_mode_bar_view);

  // Called when the settings change.
  void OnMicrophoneChanged(bool microphone_enabled);

 private:
  void OnMicrophoneToggled();

  CaptureModeSettingsEntryView* microphone_view_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_SETTINGS_VIEW_H_
