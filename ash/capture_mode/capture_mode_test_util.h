// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_

#include <string>

#include "ash/capture_mode/capture_mode_types.h"

namespace base {
class FilePath;
}  // namespace base

namespace gfx {
class Point;
}  // namespace gfx

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace views {
class View;
}  // namespace views

// Functions that are used by capture mode related unit tests and only meant to
// be used in ash_unittests.

namespace ash {

class CaptureModeController;

// Starts the capture mode session with given `source` and `type`.
CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type);

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator);

// Waits until the recording is in progress.
void WaitForRecordingToStart();

// Moves the mouse and updates the cursor's display manually to imitate what a
// real mouse move event does in shell.
// TODO(crbug.com/990589): Unit tests should be able to simulate mouse input
// without having to call |CursorManager::SetDisplay|.
void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator);

// Starts recording immediately without the 3-seconds count down.
void StartVideoRecordingImmediately();

// Returns the whole file path where the screen capture file is saved to. The
// returned file path could be either under the default downloads folder or the
// custom folder.
base::FilePath WaitForCaptureFileToBeSaved();

// Creates and returns the custom folder path. The custom folder is created in
// the default downloads folder with given `custom_folder_name`.
base::FilePath CreateCustomFolderInUserDownloadsPath(
    const std::string& custom_folder_name);

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
