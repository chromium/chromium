// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_

#include "ash/capture_mode/capture_mode_types.h"

namespace views {
class View;
}  // namespace views

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace ash {

class CaptureModeController;

// Starts the capture mode session with given `source` and `type`.
CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type);

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator);

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_