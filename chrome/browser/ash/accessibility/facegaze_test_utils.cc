// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

using FaceGazeGesture = FaceGazeTestUtils::FaceGazeGesture;
using MediapipeGesture = FaceGazeTestUtils::MediapipeGesture;

namespace {

const char* kDefaultDisplaySize = "1200x800";
constexpr char kMediapipeTestFilePath[] =
    "resources/chromeos/accessibility/accessibility_common/third_party/"
    "mediapipe_task_vision";
const int kMouseDeviceId = 1;
constexpr char kTestSupportPath[] =
    "chrome/browser/resources/chromeos/accessibility/accessibility_common/"
    "facegaze/facegaze_test_support.js";

PrefService* GetPrefs() {
  return AccessibilityManager::Get()->profile()->GetPrefs();
}

}  // namespace

FaceGazeTestUtils::Config::Config() = default;
FaceGazeTestUtils::Config::~Config() = default;

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::Default() {
  forehead_location_ = gfx::PointF(0.1, 0.2);
  cursor_location_ = gfx::Point(600, 400);
  cursor_speeds_ = {/*up=*/20, /*down=*/20, /*left=*/20, /*right=*/20};
  buffer_size_ = 1;
  use_cursor_acceleration_ = false;
  use_landmark_weights_ = false;
  use_velocity_threshold_ = false;
  dialog_accepted_ = true;

  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithForeheadLocation(
    const gfx::PointF& location) {
  forehead_location_ = location;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithCursorLocation(
    const gfx::Point& location) {
  cursor_location_ = location;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithBufferSize(int size) {
  buffer_size_ = size;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithCursorAcceleration(
    bool acceleration) {
  use_cursor_acceleration_ = acceleration;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithDialogAccepted(
    bool accepted) {
  dialog_accepted_ = accepted;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithGesturesToMacros(
    const base::flat_map<FaceGazeGesture, MacroName>& gestures_to_macros) {
  gestures_to_macros_ = std::move(gestures_to_macros);
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithGestureConfidences(
    const base::flat_map<FaceGazeGesture, int>& gesture_confidences) {
  gesture_confidences_ = std::move(gesture_confidences);
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithCursorSpeeds(
    const CursorSpeeds& speeds) {
  cursor_speeds_ = speeds;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithGestureRepeatDelayMs(
    int delay) {
  gesture_repeat_delay_ms_ = delay;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithLandmarkWeights(
    bool use_weights) {
  use_landmark_weights_ = use_weights;
  return *this;
}

FaceGazeTestUtils::Config& FaceGazeTestUtils::Config::WithVelocityThreshold(
    bool use_threshold) {
  use_velocity_threshold_ = use_threshold;
  return *this;
}

FaceGazeTestUtils::MockFaceLandmarkerResult::MockFaceLandmarkerResult() =
    default;
FaceGazeTestUtils::MockFaceLandmarkerResult::~MockFaceLandmarkerResult() =
    default;

FaceGazeTestUtils::MockFaceLandmarkerResult&
FaceGazeTestUtils::MockFaceLandmarkerResult::WithNormalizedForeheadLocation(
    const std::pair<double, double>& location) {
  forehead_location_.Set("x", location.first);
  forehead_location_.Set("y", location.second);
  return *this;
}

FaceGazeTestUtils::MockFaceLandmarkerResult&
FaceGazeTestUtils::MockFaceLandmarkerResult::WithGesture(
    const MediapipeGesture& gesture,
    int confidence) {
  // For readability and consistency with the gesture confidence pref, this
  // method accepts confidence values [0, 100]. However, the FaceLandmarker
  // receives confidence scores as values [0, 1], so we need to convert the
  // confidence to a decimal before processing it.
  recognized_gestures_.Append(
      base::Value::Dict()
          .Set("categoryName", ToString(gesture))
          .Set("score", static_cast<double>(confidence) / 100.0));
  return *this;
}

FaceGazeTestUtils::MockFaceLandmarkerResult&
FaceGazeTestUtils::MockFaceLandmarkerResult::WithLatency(int latency) {
  latency_ = latency;
  return *this;
}

FaceGazeTestUtils::FaceGazeTestUtils() = default;
FaceGazeTestUtils::~FaceGazeTestUtils() = default;

// static.
std::string FaceGazeTestUtils::ToString(const FaceGazeGesture& gesture) {
  switch (gesture) {
    case FaceGazeGesture::BROW_INNER_UP:
      return "browInnerUp";
    case FaceGazeGesture::BROWS_DOWN:
      return "browsDown";
    case FaceGazeGesture::EYE_SQUINT_LEFT:
      return "eyeSquintLeft";
    case FaceGazeGesture::EYE_SQUINT_RIGHT:
      return "eyeSquintRight";
    case FaceGazeGesture::EYES_BLINK:
      return "eyesBlink";
    case FaceGazeGesture::EYES_LOOK_DOWN:
      return "eyesLookDown";
    case FaceGazeGesture::EYES_LOOK_LEFT:
      return "eyesLookLeft";
    case FaceGazeGesture::EYES_LOOK_RIGHT:
      return "eyesLookRight";
    case FaceGazeGesture::EYES_LOOK_UP:
      return "eyesLookUp";
    case FaceGazeGesture::JAW_LEFT:
      return "jawLeft";
    case FaceGazeGesture::JAW_OPEN:
      return "jawOpen";
    case FaceGazeGesture::JAW_RIGHT:
      return "jawRight";
    case FaceGazeGesture::MOUTH_FUNNEL:
      return "mouthFunnel";
    case FaceGazeGesture::MOUTH_LEFT:
      return "mouthLeft";
    case FaceGazeGesture::MOUTH_PUCKER:
      return "mouthPucker";
    case FaceGazeGesture::MOUTH_RIGHT:
      return "mouthRight";
    case FaceGazeGesture::MOUTH_SMILE:
      return "mouthSmile";
    case FaceGazeGesture::MOUTH_UPPER_UP:
      return "mouthUpperUp";
  }
}

// static.
std::string FaceGazeTestUtils::ToString(const MediapipeGesture& gesture) {
  switch (gesture) {
    case MediapipeGesture::BROW_DOWN_LEFT:
      return "browDownLeft";
    case MediapipeGesture::BROW_DOWN_RIGHT:
      return "browDownRight";
    case MediapipeGesture::BROW_INNER_UP:
      return "browInnerUp";
    case MediapipeGesture::EYE_BLINK_LEFT:
      return "eyeBlinkLeft";
    case MediapipeGesture::EYE_BLINK_RIGHT:
      return "eyeBlinkRight";
    case MediapipeGesture::EYE_LOOK_DOWN_LEFT:
      return "eyeLookDownLeft";
    case MediapipeGesture::EYE_LOOK_DOWN_RIGHT:
      return "eyeLookDownRight";
    case MediapipeGesture::EYE_LOOK_IN_LEFT:
      return "eyeLookInLeft";
    case MediapipeGesture::EYE_LOOK_IN_RIGHT:
      return "eyeLookInRight";
    case MediapipeGesture::EYE_LOOK_OUT_LEFT:
      return "eyeLookOutLeft";
    case MediapipeGesture::EYE_LOOK_OUT_RIGHT:
      return "eyeLookOutRight";
    case MediapipeGesture::EYE_LOOK_UP_LEFT:
      return "eyeLookUpLeft";
    case MediapipeGesture::EYE_LOOK_UP_RIGHT:
      return "eyeLookUpRight";
    case MediapipeGesture::EYE_SQUINT_LEFT:
      return "eyeSquintLeft";
    case MediapipeGesture::EYE_SQUINT_RIGHT:
      return "eyeSquintRight";
    case MediapipeGesture::JAW_LEFT:
      return "jawLeft";
    case MediapipeGesture::JAW_OPEN:
      return "jawOpen";
    case MediapipeGesture::JAW_RIGHT:
      return "jawRight";
    case MediapipeGesture::MOUTH_FUNNEL:
      return "mouthFunnel";
    case MediapipeGesture::MOUTH_LEFT:
      return "mouthLeft";
    case MediapipeGesture::MOUTH_PUCKER:
      return "mouthPucker";
    case MediapipeGesture::MOUTH_RIGHT:
      return "mouthRight";
    case MediapipeGesture::MOUTH_SMILE_LEFT:
      return "mouthSmileLeft";
    case MediapipeGesture::MOUTH_SMILE_RIGHT:
      return "mouthSmileRight";
    case MediapipeGesture::MOUTH_UPPER_UP_LEFT:
      return "mouthUpperUpLeft";
    case MediapipeGesture::MOUTH_UPPER_UP_RIGHT:
      return "mouthUpperUpRight";
  }
}

void FaceGazeTestUtils::EnableFaceGaze(const Config& config) {
  // TODO(b/309121742): Add display size to Config so that tests can configure
  // it.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .UpdateDisplay(kDefaultDisplaySize);
  event_generator_ = std::make_unique<ui::test::EventGenerator>(
      Shell::Get()->GetPrimaryRootWindow());

  // Before enabling FaceGaze, ensure that the dialog accepted pref matches
  // what is specified in the config.
  GetPrefs()->SetBoolean(
      prefs::kAccessibilityFaceGazeAcceleratorDialogHasBeenAccepted,
      config.dialog_accepted());

  FaceGazeTestUtils::SetUpMediapipeDir();
  ASSERT_FALSE(AccessibilityManager::Get()->IsFaceGazeEnabled());

  // Use ExtensionHostTestHelper to detect when the accessibility common
  // extension loads.
  extensions::ExtensionHostTestHelper host_helper(
      AccessibilityManager::Get()->profile(),
      extension_misc::kAccessibilityCommonExtensionId);
  AccessibilityManager::Get()->EnableFaceGaze(true);
  host_helper.WaitForHostCompletedFirstLoad();

  WaitForJSReady();
  SetUpJSTestSupport();
  if (config.dialog_accepted()) {
    // The FaceLandmarker will be automatically initialized after the dialog has
    // been accepted.
    WaitForFaceLandmarker();
  }

  CancelMouseControllerInterval();
  ConfigureFaceGaze(config);
}

void FaceGazeTestUtils::WaitForCursorPosition(const gfx::Point& location) {
  std::string script =
      base::StringPrintf("faceGazeTestSupport.waitForCursorLocation(%d, %d);",
                         location.x(), location.y());
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::ProcessFaceLandmarkerResult(
    const MockFaceLandmarkerResult& result) {
  std::string forehead_location_json =
      base::WriteJson(result.forehead_location()).value();
  std::string recognized_gestures_json =
      base::WriteJson(result.recognized_gestures()).value();
  std::string script;
  if (result.latency().has_value()) {
    script = base::StringPrintf(
        "faceGazeTestSupport.processFaceLandmarkerResult(%s, %s, %d)",
        forehead_location_json.c_str(), recognized_gestures_json.c_str(),
        result.latency().value());
  } else {
    script = base::StringPrintf(
        "faceGazeTestSupport.processFaceLandmarkerResult(%s, %s)",
        forehead_location_json.c_str(), recognized_gestures_json.c_str());
  }

  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::TriggerMouseControllerInterval() {
  std::string script = "faceGazeTestSupport.triggerMouseControllerInterval();";
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::MoveMouseTo(const gfx::Point& location) {
  event_generator_->MoveMouseTo(location.x(), location.y());
}

void FaceGazeTestUtils::AssertCursorAt(const gfx::Point& location) {
  WaitForCursorPosition(location);
  ASSERT_EQ(location, display::Screen::GetScreen()->GetCursorScreenPoint());
}

void FaceGazeTestUtils::AssertScrollMode(bool active) {
  std::string true_script = "faceGazeTestSupport.assertScrollMode(true);";
  std::string false_script = "faceGazeTestSupport.assertScrollMode(false);";
  ExecuteAccessibilityCommonScript(active ? true_script : false_script);
}

void FaceGazeTestUtils::ExecuteAccessibilityCommonScript(
    const std::string& script) {
  extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      /*context=*/AccessibilityManager::Get()->profile(),
      /*extension_id=*/extension_misc::kAccessibilityCommonExtensionId,
      /*script=*/script);
}

void FaceGazeTestUtils::SetUpMediapipeDir() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath gen_root_dir;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &gen_root_dir));
  base::FilePath test_file_path =
      gen_root_dir.AppendASCII(kMediapipeTestFilePath);
  ASSERT_TRUE(base::PathExists(test_file_path));
  AccessibilityManager::Get()->SetDlcPathForTest(test_file_path);
}

void FaceGazeTestUtils::WaitForJSReady() {
  std::string script = base::StringPrintf(R"JS(
    (async function() {
      window.accessibilityCommon.setFeatureLoadCallbackForTest('facegaze',
          () => {
            chrome.test.sendScriptResult('ready');
          });
    })();
  )JS");
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::SetUpJSTestSupport() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath source_dir;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_dir));
  auto test_support_path = source_dir.AppendASCII(kTestSupportPath);
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(test_support_path, &script))
      << test_support_path;
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::CancelMouseControllerInterval() {
  std::string script = "faceGazeTestSupport.cancelMouseControllerInterval();";
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::WaitForFaceLandmarker() {
  std::string script = "faceGazeTestSupport.waitForFaceLandmarker();";
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::ConfigureFaceGaze(const Config& config) {
  // Set optional configuration properties.
  if (config.cursor_speeds().has_value()) {
    SetCursorSpeeds(config.cursor_speeds().value());
  }
  if (config.gestures_to_macros().has_value()) {
    SetGesturesToMacros(config.gestures_to_macros().value());
  }
  if (config.gesture_confidences().has_value()) {
    SetGestureConfidences(config.gesture_confidences().value());
  }
  if (config.gesture_repeat_delay_ms().has_value()) {
    SetGestureRepeatDelayMs(config.gesture_repeat_delay_ms().value());
  }

  // Set required configuration properties.
  SetBufferSize(config.buffer_size());
  SetCursorAcceleration(config.use_cursor_acceleration());
  SetLandmarkWeights(config.use_landmark_weights());
  SetVelocityThreshold(config.use_velocity_threshold());

  // By default the cursor is placed at the center of the screen. To
  // initialize FaceGaze, move the cursor somewhere, then move it to the
  // location specified by the config.
  event_generator_->set_mouse_source_device_id(kMouseDeviceId);
  MoveMouseTo(gfx::Point(0, 0));
  AssertCursorAt(gfx::Point(0, 0));
  MoveMouseTo(config.cursor_location());
  AssertCursorAt(config.cursor_location());

  // TODO(b/309121742): only call ProcessFaceLandmarkerResult if the forehead
  // location is specified by the config.
  // No matter the starting location, the cursor position won't change
  // initially, and upcoming forehead locations will be computed relative to
  // this.
  ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult().WithNormalizedForeheadLocation(std::make_pair(
          config.forehead_location().x(), config.forehead_location().y())));
  TriggerMouseControllerInterval();
  AssertCursorAt(config.cursor_location());
}

void FaceGazeTestUtils::SetCursorSpeeds(const CursorSpeeds& speeds) {
  GetPrefs()->SetInteger(prefs::kAccessibilityFaceGazeCursorSpeedUp, speeds.up);
  GetPrefs()->SetInteger(prefs::kAccessibilityFaceGazeCursorSpeedDown,
                         speeds.down);
  GetPrefs()->SetInteger(prefs::kAccessibilityFaceGazeCursorSpeedLeft,
                         speeds.left);
  GetPrefs()->SetInteger(prefs::kAccessibilityFaceGazeCursorSpeedRight,
                         speeds.right);
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::SetBufferSize(int size) {
  GetPrefs()->SetInteger(prefs::kAccessibilityFaceGazeCursorSmoothing, size);
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::SetCursorAcceleration(bool use_acceleration) {
  GetPrefs()->SetBoolean(prefs::kAccessibilityFaceGazeCursorUseAcceleration,
                         use_acceleration);
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::SetLandmarkWeights(bool use_weights) {
  std::string true_script = "faceGazeTestSupport.setLandmarkWeights(true);";
  std::string false_script = "faceGazeTestSupport.setLandmarkWeights(false);";
  ExecuteAccessibilityCommonScript(use_weights ? true_script : false_script);
}

void FaceGazeTestUtils::SetVelocityThreshold(bool use_threshold) {
  // TODO(b/309121742): Update this to set the pref value after a pref for
  // velocity threshold has been added.
  std::string true_script = "faceGazeTestSupport.setVelocityThreshold(true);";
  std::string false_script = "faceGazeTestSupport.setVelocityThreshold(false);";
  ExecuteAccessibilityCommonScript(use_threshold ? true_script : false_script);
}

void FaceGazeTestUtils::SetGesturesToMacros(
    const base::flat_map<FaceGazeGesture, MacroName>& gestures_to_macros) {
  // Copy the stricly-typed mapping of gestures to macros into a dictionary
  // value that can be used as the preference value.
  base::Value::Dict dict;
  for (const auto& mapping : gestures_to_macros) {
    dict.Set(ToString(mapping.first), mapping.second);
  }
  GetPrefs()->SetDict(prefs::kAccessibilityFaceGazeGesturesToMacros,
                      std::move(dict));
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::SetGestureConfidences(
    const base::flat_map<FaceGazeGesture, int>& gesture_confidences) {
  // Copy the stricly-typed mapping of gestures to confidences into a dictionary
  // value that can be used as the preference value.
  base::Value::Dict dict;
  for (const auto& mapping : gesture_confidences) {
    dict.Set(ToString(mapping.first), mapping.second);
  }
  GetPrefs()->SetDict(prefs::kAccessibilityFaceGazeGesturesToConfidence,
                      std::move(dict));
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::SetGestureRepeatDelayMs(int delay) {
  std::string script = base::StringPrintf(
      "faceGazeTestSupport.setGestureRepeatDelayMs(%d);", delay);
  ExecuteAccessibilityCommonScript(script);
}

}  // namespace ash
