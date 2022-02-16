// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_

#include "ash/capture_mode/capture_mode_types.h"

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

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
