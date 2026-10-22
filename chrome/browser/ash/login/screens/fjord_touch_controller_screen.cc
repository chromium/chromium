// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/fjord_touch_controller_screen.h"

#include <string>

#include "base/system/sys_info.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ui/webui/ash/login/fjord_touch_controller_screen_handler.h"

namespace ash {

namespace {

constexpr char kChromeOSReleaseTrack[] = "CHROMEOS_RELEASE_TRACK";
constexpr char kTestImageRelease[] = "testimage-channel";

// Returns true if the device is running a test image.
bool IsTestImage() {
  std::string track;
  return base::SysInfo::GetLsbReleaseValue(kChromeOSReleaseTrack, &track) &&
         track.find(kTestImageRelease) != std::string::npos;
}

}  // namespace

FjordTouchControllerScreen::FjordTouchControllerScreen(
    base::WeakPtr<FjordTouchControllerScreenView> view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(FjordTouchControllerScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      OobeMojoBinder(this),
      exit_callback_(exit_callback),
      view_(std::move(view)) {}

FjordTouchControllerScreen::~FjordTouchControllerScreen() = default;

void FjordTouchControllerScreen::ShowImpl() {
  is_test_image_ = IsTestImage();
  // Remote/frontend is missing during some tests.
  if (GetRemote()->is_bound()) {
    (*GetRemote())->SetTestImageSkipButtonVisible(is_test_image_);
  }
  UpdateNextButtonState();
  view_->Show();
}

bool FjordTouchControllerScreen::ExitScreen() {
  if (is_hidden()) {
    return false;
  }

  exit_callback_.Run();
  return true;
}

bool FjordTouchControllerScreen::OnTouchControllerSetupFinished() {
  if (is_hidden()) {
    return false;
  }

  touch_controller_setup_finished_ = true;
  UpdateNextButtonState();
  return true;
}

void FjordTouchControllerScreen::UpdateNextButtonState() {
  // Remote/frontend is missing during some tests.
  if (GetRemote()->is_bound()) {
    (*GetRemote())->SetNextButtonEnabled(touch_controller_setup_finished_);
  }
}

void FjordTouchControllerScreen::OnSetupComplete() {
  // The next button should only be clickable after the touch controller setup
  // is finished.
  if (!touch_controller_setup_finished_) {
    return;
  }
  ExitScreen();
}

void FjordTouchControllerScreen::OnTestImageSkipClicked() {
  // The skip button is only available on test images.
  if (!is_test_image_) {
    return;
  }
  ExitScreen();
}

}  // namespace ash
