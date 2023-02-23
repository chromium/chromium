// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_

#include <string>

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/user_nudge_controller.h"
#include "ash/public/cpp/test/mock_projector_client.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

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

class IconButton;
class CaptureModeController;
class CaptureModeBarView;

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

// Sends a press release key combo `count` times.
void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator,
             int flags = ui::EF_NONE,
             int count = 1);

// Wait for a specific `seconds`.
void WaitForSeconds(int seconds);

// To avoid flaky failures due to mouse devices blocking entering tablet mode,
// we detach all mouse devices. This shouldn't affect testing the cursor
// status.
void SwitchToTabletMode();

// Open the `view` by touch.
void TouchOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator);

// Clicks or taps on the `view` based on whether the user is in clamshell or
// tablet mode.
void ClickOrTapView(const views::View* view,
                    bool in_table_mode,
                    ui::test::EventGenerator* event_generator);

CaptureModeBarView* GetCaptureModeBarView();

IconButton* GetFullscreenToggleButton();

IconButton* GetRegionToggleButton();

UserNudgeController* GetUserNudgeController();

bool IsLayerStackedRightBelow(ui::Layer* layer, ui::Layer* sibling);

// Sets the device scale factor for only the first available display.
void SetDeviceScaleFactor(float dsf);

// Enables the auto click accessibility feature, and returns the auto click
// bubble widget.
views::Widget* EnableAndGetAutoClickBubbleWidget();

// Functions to simulate triggering key events from the virtual keyboard.
void PressKeyOnVK(ui::test::EventGenerator* event_generator,
                  ui::KeyboardCode key_code,
                  int flags,
                  int source_device_id = ui::ED_UNKNOWN_DEVICE);
void ReleaseKeyOnVK(ui::test::EventGenerator* event_generator,
                    ui::KeyboardCode key_code,
                    int flags,
                    int source_device_id = ui::ED_UNKNOWN_DEVICE);
void PressAndReleaseKeyOnVK(ui::test::EventGenerator* event_generator,
                            ui::KeyboardCode key_code,
                            int flags = ui::EF_NONE,
                            int source_device_id = ui::ED_UNKNOWN_DEVICE);

// Defines a helper class to allow setting up and testing the Projector feature
// in multiple test fixtures. Note that this helper initializes the Projector-
// related features in its constructor, so test fixtures that use this should
// also initialize their `ScopedFeatureList` in their constructors to avoid
// DCHECKing when nested ScopedFeatureLists being destroyed in a different order
// than they are initialized.
class ProjectorCaptureModeIntegrationHelper {
 public:
  ProjectorCaptureModeIntegrationHelper();
  ProjectorCaptureModeIntegrationHelper(
      const ProjectorCaptureModeIntegrationHelper&) = delete;
  ProjectorCaptureModeIntegrationHelper& operator=(
      const ProjectorCaptureModeIntegrationHelper&) = delete;
  ~ProjectorCaptureModeIntegrationHelper() = default;

  MockProjectorClient* projector_client() { return &projector_client_; }

  // Sets up the projector feature. Must be called after `AshTestBase::SetUp()`
  // has been called.
  void SetUp();

  bool CanStartProjectorSession() const;

  // Starts a new projector capture session.
  void StartProjectorModeSession();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  MockProjectorClient projector_client_;
};

// Defines a waiter to observe the visibility change of the view.
class ViewVisibilityChangeWaiter : public views::ViewObserver {
 public:
  explicit ViewVisibilityChangeWaiter(views::View* view);
  ViewVisibilityChangeWaiter(const ViewVisibilityChangeWaiter&) = delete;
  ViewVisibilityChangeWaiter& operator=(const ViewVisibilityChangeWaiter&) =
      delete;
  ~ViewVisibilityChangeWaiter() override;

  void Wait();

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

 private:
  views::View* const view_;
  base::RunLoop wait_loop_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_TEST_UTIL_H_
