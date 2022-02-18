// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_test_util.h"

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/test_capture_mode_delegate.h"
#include "ash/public/cpp/capture_mode/capture_mode_test_api.h"
#include "ash/shell.h"
#include "ash/wm/cursor_manager_chromeos.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view.h"

namespace ash {

CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                           CaptureModeType type) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(source);
  controller->SetType(type);
  controller->Start(CaptureModeEntryType::kQuickSettings);
  DCHECK(controller->IsActive());
  return controller;
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void WaitForRecordingToStart() {
  auto* controller = CaptureModeController::Get();
  if (controller->is_recording_in_progress())
    return;
  auto* test_delegate =
      static_cast<TestCaptureModeDelegate*>(controller->delegate_for_testing());
  ASSERT_TRUE(test_delegate);
  base::RunLoop run_loop;
  test_delegate->set_on_recording_started_callback(run_loop.QuitClosure());
  run_loop.Run();
  ASSERT_TRUE(controller->is_recording_in_progress());
}

void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

void StartVideoRecordingImmediately() {
  CaptureModeController::Get()->StartVideoRecordingImmediatelyForTesting();
  WaitForRecordingToStart();
}

base::FilePath WaitForCaptureFileToBeSaved() {
  base::FilePath result;
  base::RunLoop run_loop;
  ash::CaptureModeTestApi().SetOnCaptureFileSavedCallback(
      base::BindLambdaForTesting([&](const base::FilePath& path) {
        result = path;
        run_loop.Quit();
      }));
  run_loop.Run();
  return result;
}

base::FilePath CreateCustomFolderInUserDownloadsPath(
    const std::string& custom_folder_name) {
  base::FilePath custom_folder = CaptureModeController::Get()
                                     ->delegate_for_testing()
                                     ->GetUserDefaultDownloadsFolder()
                                     .Append(custom_folder_name);
  base::ScopedAllowBlockingForTesting allow_blocking;
  const bool result = base::CreateDirectory(custom_folder);
  DCHECK(result);
  return custom_folder;
}

}  // namespace ash
