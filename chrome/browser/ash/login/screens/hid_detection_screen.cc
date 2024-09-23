// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/hid_detection_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/hid_detection_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/hid_detection/hid_detection_manager_impl.h"
#include "chromeos/ash/components/hid_detection/hid_detection_utils.h"
#include "chromeos/constants/devicetype.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/device_service.h"
#include "ui/base/l10n/l10n_util.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace ash {

namespace {

// Possible ui-states for device-blocks.
constexpr char kSearchingState[] = "searching";
constexpr char kUSBState[] = "usb";
constexpr char kConnectedState[] = "connected";
constexpr char kBTPairedState[] = "paired";
constexpr char kBTPairingState[] = "pairing";
constexpr char kUserActionContinue[] = "HIDDetectionOnContinue";

std::unique_ptr<hid_detection::HidDetectionManager>&
GetHidDetectionManagerOverrideForTesting() {
  static base::NoDestructor<std::unique_ptr<hid_detection::HidDetectionManager>>
      hid_detection_manager;
  return *hid_detection_manager;
}

std::string GetDeviceUiState(
    const hid_detection::HidDetectionManager::InputState& state) {
  switch (state) {
    case hid_detection::HidDetectionManager::InputState::kSearching:
      return kSearchingState;
    case hid_detection::HidDetectionManager::InputState::kConnectedViaUsb:
      return kUSBState;
    case hid_detection::HidDetectionManager::InputState::kPairingViaBluetooth:
      return kBTPairingState;
    case hid_detection::HidDetectionManager::InputState::kPairedViaBluetooth:
      return kBTPairedState;
    case hid_detection::HidDetectionManager::InputState::kConnected:
      return kConnectedState;
  }
}

bool IsInputConnected(
    const hid_detection::HidDetectionManager::InputMetadata& metadata) {
  return metadata.state !=
             hid_detection::HidDetectionManager::InputState::kSearching &&
         metadata.state != hid_detection::HidDetectionManager::InputState::
                               kPairingViaBluetooth;
}

}  // namespace

// static
std::string HIDDetectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::SKIPPED_FOR_TESTS:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

bool HIDDetectionScreen::CanShowScreen() {
  if (StartupUtils::IsHIDDetectionScreenDisabledForTests() ||
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableHIDDetectionOnOOBEForTesting)) {
    // Store the flag inside the local state so it persists restart for the
    // autoupdate tests.
    StartupUtils::DisableHIDDetectionScreenForTests();
    return false;
  }

  switch (chromeos::GetDeviceType()) {
    case chromeos::DeviceType::kChromebase:
    case chromeos::DeviceType::kChromebit:
    case chromeos::DeviceType::kChromebox:
      return true;
    default:
      return false;
  }
}

HIDDetectionScreen::HIDDetectionScreen(base::WeakPtr<HIDDetectionView> view,
                                       const ScreenExitCallback& exit_callback)
    : BaseScreen(HIDDetectionView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {
  const auto& hid_detection_manager_override =
      GetHidDetectionManagerOverrideForTesting();
  hid_detection_manager_ =
      hid_detection_manager_override
          ? std::unique_ptr<hid_detection::HidDetectionManager>(
                hid_detection_manager_override.get())
          : std::make_unique<hid_detection::HidDetectionManagerImpl>(
                &content::GetDeviceService());
}

HIDDetectionScreen::~HIDDetectionScreen() = default;

// static
void HIDDetectionScreen::OverrideHidDetectionManagerForTesting(
    std::unique_ptr<hid_detection::HidDetectionManager> hid_detection_manager) {
  GetHidDetectionManagerOverrideForTesting() = std::move(hid_detection_manager);
}

void HIDDetectionScreen::OnContinueButtonClicked() {
  hid_detection_manager_->StopHidDetection();
  Exit(Result::NEXT);
}

void HIDDetectionScreen::CheckIsScreenRequired(
    base::OnceCallback<void(bool)> on_check_done) {
    hid_detection_manager_->GetIsHidDetectionRequired(std::move(on_check_done));
}

bool HIDDetectionScreen::MaybeSkip(WizardContext& context) {
  if (!CanShowScreen()) {
    // TODO(https://crbug.com/1275960): Introduce Result::SKIPPED.
    Exit(Result::SKIPPED_FOR_TESTS);
    return true;
  }

  return false;
}

void HIDDetectionScreen::ShowImpl() {
  if (!is_hidden())
    return;

  if (view_)
    view_->Show();

  hid_detection_manager_->StartHidDetection(/*delegate=*/this);
}

void HIDDetectionScreen::HideImpl() {}

void HIDDetectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionContinue) {
    OnContinueButtonClicked();
    return;
  }
  BaseScreen::OnUserAction(args);
}

void HIDDetectionScreen::Exit(Result result) {
  exit_result_for_testing_ = result;
  exit_callback_.Run(result);
}

void HIDDetectionScreen::OnHidDetectionStatusChanged(
    hid_detection::HidDetectionManager::HidDetectionStatus status) {
  if (!view_)
    return;

  view_->SetTouchscreenDetectedState(status.touchscreen_detected);
  view_->SetMouseState(GetDeviceUiState(status.pointer_metadata.state));
  view_->SetPointingDeviceName(status.pointer_metadata.detected_hid_name);

  std::string keyboard_state = GetDeviceUiState(status.keyboard_metadata.state);

  // Unlike pointing devices, which can be connected through serial IO or some
  // other medium, keyboards only report USB and Bluetooth states.
  if (keyboard_state == kConnectedState)
    keyboard_state = kUSBState;
  view_->SetKeyboardState(keyboard_state);
  view_->SetKeyboardDeviceName(status.keyboard_metadata.detected_hid_name);
  view_->SetContinueButtonEnabled(status.touchscreen_detected ||
                                  IsInputConnected(status.pointer_metadata) ||
                                  IsInputConnected(status.keyboard_metadata));

  view_->SetPinDialogVisible(status.pairing_state.has_value());
  if (status.pairing_state.has_value()) {
    view_->SetKeyboardPinCode(status.pairing_state.value().code);
    view_->SetNumKeysEnteredPinCode(
        status.pairing_state.value().num_keys_entered);
    return;
  }
}
}  // namespace ash
