// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"

#include "ash/constants/ash_pref_names.h"
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
#include "ui/gfx/geometry/point.h"

namespace ash {

namespace {

constexpr char kMediapipeTestFilePath[] =
    "resources/chromeos/accessibility/accessibility_common/third_party/"
    "mediapipe_task_vision";
constexpr char kTestSupportPath[] =
    "chrome/browser/resources/chromeos/accessibility/accessibility_common/"
    "facegaze/facegaze_test_support.js";

PrefService* GetPrefs() {
  return AccessibilityManager::Get()->profile()->GetPrefs();
}

}  // namespace

FaceGazeTestUtils::MockFaceLandmarkerResult::MockFaceLandmarkerResult() =
    default;
FaceGazeTestUtils::MockFaceLandmarkerResult::~MockFaceLandmarkerResult() =
    default;

FaceGazeTestUtils::MockFaceLandmarkerResult&
FaceGazeTestUtils::MockFaceLandmarkerResult::WithNormalizedForeheadLocation(
    double x,
    double y) {
  forehead_location_.Set("x", x);
  forehead_location_.Set("y", y);
  return *this;
}

FaceGazeTestUtils::MockFaceLandmarkerResult&
FaceGazeTestUtils::MockFaceLandmarkerResult::WithGesture(
    const std::string& gesture,
    double confidence) {
  recognized_gestures_.Append(base::Value::Dict()
                                  .Set("categoryName", gesture)
                                  .Set("score", confidence));
  return *this;
}

FaceGazeTestUtils::FaceGazeTestUtils() = default;
FaceGazeTestUtils::~FaceGazeTestUtils() = default;

void FaceGazeTestUtils::EnableFaceGaze() {
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
  CancelMouseControllerInterval();
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

void FaceGazeTestUtils::ExecuteAccessibilityCommonScript(
    const std::string& script) {
  extensions::browsertest_util::ExecuteScriptInBackgroundPage(
      /*context=*/AccessibilityManager::Get()->profile(),
      /*extension_id=*/extension_misc::kAccessibilityCommonExtensionId,
      /*script=*/script);
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

void FaceGazeTestUtils::CreateFaceLandmarker() {
  std::string script = "faceGazeTestSupport.createFaceLandmarker();";
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::WaitForMousePosition(const gfx::Point& location) {
  std::string script =
      base::StringPrintf("faceGazeTestSupport.waitForMouseLocation(%d, %d);",
                         location.x(), location.y());
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::InitializeBufferSizeAndAcceleration(int size) {
  GetPrefs()->SetInteger(prefs::kAccessibilityFaceGazeCursorSmoothing, size);
  GetPrefs()->SetBoolean(prefs::kAccessibilityFaceGazeCursorUseAcceleration,
                         false);
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::InitializeGesturesToMacros(
    const base::Value::Dict& gestures_to_macros) {
  GetPrefs()->SetDict(prefs::kAccessibilityFaceGazeGesturesToMacros,
                      gestures_to_macros.Clone());
  GetPrefs()->CommitPendingWrite();
}

void FaceGazeTestUtils::ProcessFaceLandmarkerResult(
    const MockFaceLandmarkerResult& result) {
  std::string forehead_location_json =
      base::WriteJson(result.forehead_location()).value();
  std::string recognized_gestures_json =
      base::WriteJson(result.recognized_gestures()).value();
  std::string script = base::StringPrintf(
      "faceGazeTestSupport.processFaceLandmarkerResult(%s, %s)",
      forehead_location_json.c_str(), recognized_gestures_json.c_str());
  ExecuteAccessibilityCommonScript(script);
}

void FaceGazeTestUtils::TriggerMouseControllerInterval() {
  std::string script = "faceGazeTestSupport.triggerMouseControllerInterval();";
  ExecuteAccessibilityCommonScript(script);
}

}  // namespace ash
