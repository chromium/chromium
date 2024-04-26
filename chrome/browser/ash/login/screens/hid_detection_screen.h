// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// Representation independent class that controls screen showing warning about
// HID absence to users.
class HIDDetectionScreen : public BaseScreen,
                           public hid_detection::HidDetectionManager::Delegate {
 public:
  using TView = HIDDetectionView;

  enum class Result { NEXT, SKIPPED_FOR_TESTS };

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  HIDDetectionScreen(base::WeakPtr<HIDDetectionView> view,
                     const ScreenExitCallback& exit_callback);

  HIDDetectionScreen(const HIDDetectionScreen&) = delete;
  HIDDetectionScreen& operator=(const HIDDetectionScreen&) = delete;

  ~HIDDetectionScreen() override;

  static std::string GetResultString(Result result);

  // The HID detection screen is only allowed for form factors without built-in
  // inputs: Chromebases, Chromebits, and Chromeboxes (crbug.com/965765).
  // Also different testing flags might forcefully skip the screen
  static bool CanShowScreen();

  // Checks if this screen should be displayed. `on_check_done` should be
  // invoked with the result; true if the screen should be displayed, false
  // otherwise.
  void CheckIsScreenRequired(base::OnceCallback<void(bool)> on_check_done);

  // Allows tests to override what HidDetectionManager implementation is used.
  static void OverrideHidDetectionManagerForTesting(
      std::unique_ptr<hid_detection::HidDetectionManager>
          hid_detection_manager);

  const std::optional<Result>& get_exit_result_for_testing() const {
    return exit_result_for_testing_;
  }

 private:
  friend class HIDDetectionScreenChromeboxTest;

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // hid_detection::HidDetectionManager::Delegate:
  void OnHidDetectionStatusChanged(
      hid_detection::HidDetectionManager::HidDetectionStatus status) override;

  // Called when continue button was clicked.
  void OnContinueButtonClicked();

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // pointing device.
  void SendPointingDeviceNotification();

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // keyboard device.
  void SendKeyboardDeviceNotification();

  // Sends a notification to the Web UI of the status of available Touch Screen
  void SendTouchScreenDeviceNotification();

  void Exit(Result result);

  base::WeakPtr<HIDDetectionView> view_;

  const ScreenExitCallback exit_callback_;
  std::optional<Result> exit_result_for_testing_;

  std::unique_ptr<hid_detection::HidDetectionManager> hid_detection_manager_;

  base::WeakPtrFactory<HIDDetectionScreen> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
