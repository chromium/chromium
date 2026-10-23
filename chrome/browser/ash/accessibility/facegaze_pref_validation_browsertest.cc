// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/facegaze_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

using Config = FaceGazeTestUtils::Config;
using FaceGazeGesture = FaceGazeTestUtils::FaceGazeGesture;
using MacroName = FaceGazeTestUtils::MacroName;
using MediapipeGesture = FaceGazeTestUtils::MediapipeGesture;
using MockFaceLandmarkerResult = FaceGazeTestUtils::MockFaceLandmarkerResult;

constexpr std::pair<double, double> kDefaultForeheadLocation =
    std::make_pair(0.11, 0.21);

aura::Window* GetRootWindow() {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window) {
    root_window = Shell::GetPrimaryRootWindow();
  }
  return root_window;
}

class MockEventHandler : public ui::EventHandler {
 public:
  MockEventHandler() = default;
  ~MockEventHandler() override = default;
  MockEventHandler(const MockEventHandler&) = delete;
  MockEventHandler& operator=(const MockEventHandler&) = delete;

  void OnKeyEvent(ui::KeyEvent* event) override {
    key_events_.push_back(*event);
  }

  void ClearEvents() { key_events_.clear(); }

  const std::vector<ui::KeyEvent>& key_events() const { return key_events_; }

 private:
  std::vector<ui::KeyEvent> key_events_;
};

// TODO(crbug.com/486105659): Merge these tests into facegaze_browsertest.cc
// once that file is re-enabled in chrome/browser/ash/accessibility/BUILD.gn.
class FaceGazePrefValidationBrowserTest
    : public AccessibilityFeatureBrowserTest {
 public:
  FaceGazePrefValidationBrowserTest() = default;
  ~FaceGazePrefValidationBrowserTest() override = default;
  FaceGazePrefValidationBrowserTest(const FaceGazePrefValidationBrowserTest&) =
      delete;
  FaceGazePrefValidationBrowserTest& operator=(
      const FaceGazePrefValidationBrowserTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    AccessibilityFeatureBrowserTest::SetUpOnMainThread();
    utils_ = std::make_unique<FaceGazeTestUtils>();
    root_window_ = GetRootWindow();
    root_window_->AddPreTargetHandler(&event_handler_);
  }

  void TearDownOnMainThread() override {
    if (root_window_) {
      root_window_->RemovePreTargetHandler(&event_handler_);
      root_window_ = nullptr;
    }
    AccessibilityFeatureBrowserTest::TearDownOnMainThread();
  }

  PrefService* GetPrefs() {
    return AccessibilityManager::Get()->profile()->GetPrefs();
  }

  FaceGazeTestUtils* utils() { return utils_.get(); }
  MockEventHandler& event_handler() { return event_handler_; }

 private:
  std::unique_ptr<FaceGazeTestUtils> utils_;
  raw_ptr<aura::Window> root_window_ = nullptr;
  MockEventHandler event_handler_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(FaceGazePrefValidationBrowserTest,
                       MissingCustomKeyComboDoesNotFireOrCrash) {
  // Bind browInnerUp to CUSTOM_KEY_COMBINATION with no corresponding key
  // combination entry, alongside a valid jawOpen -> KEY_PRESS_SPACE binding in
  // the same frame. Ensure there is no uncaught error when browInnerUp is
  // detected and that jawOpen still executes.
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      {{FaceGazeGesture::BROW_INNER_UP, MacroName::CUSTOM_KEY_COMBINATION},
       {FaceGazeGesture::JAW_OPEN, MacroName::KEY_PRESS_SPACE}},
      {{FaceGazeGesture::BROW_INNER_UP, 1}, {FaceGazeGesture::JAW_OPEN, 70}}));

  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithNormalizedForeheadLocation(kDefaultForeheadLocation)
          .WithGesture(MediapipeGesture::BROW_INNER_UP, 90)
          .WithGesture(MediapipeGesture::JAW_OPEN, 90));

  // Ensure the incomplete browInnerUp binding is skipped and jawOpen ->
  // KEY_PRESS_SPACE fires.
  ASSERT_EQ(1u, event_handler().key_events().size());
  EXPECT_EQ(ui::KeyboardCode::VKEY_SPACE,
            event_handler().key_events()[0].key_code());
  EXPECT_EQ(ui::EventType::kKeyPressed, event_handler().key_events()[0].type());
}

IN_PROC_BROWSER_TEST_F(FaceGazePrefValidationBrowserTest,
                       InvalidPreferencesAreIgnoredOrClamped) {
  utils()->EnableFaceGaze(Config().Default().WithBindings(
      {{FaceGazeGesture::JAW_OPEN, MacroName::KEY_PRESS_SPACE},
       {FaceGazeGesture::JAW_LEFT, MacroName::KEY_PRESS_LEFT},
       {FaceGazeGesture::JAW_RIGHT, MacroName::KEY_PRESS_RIGHT}},
      {{FaceGazeGesture::JAW_OPEN, 70},
       {FaceGazeGesture::JAW_LEFT, 70},
       {FaceGazeGesture::JAW_RIGHT, 90}}));

  // Inject out-of-range confidences (0, 150, -50), non-numeric confidence
  // ("notANumber"), invalid macro IDs (999999), non-numeric macro values
  // (true), invalid gesture names ("notAGesture"), malformed JSON, invalid key
  // code (99999), and unknown key code (0 = UNKNOWN).
  GetPrefs()->SetDict(prefs::kAccessibilityFaceGazeGesturesToMacros,
                      base::DictValue()
                          .Set("notAGesture", 51)
                          .Set("browInnerUp", 999999)
                          .Set("mouthFunnel", true)
                          .Set("jawOpen", MacroName::KEY_PRESS_SPACE)
                          .Set("jawLeft", MacroName::KEY_PRESS_LEFT)
                          .Set("jawRight", MacroName::KEY_PRESS_RIGHT)
                          .Set("mouthPucker", 51)
                          .Set("eyeSquintLeft", 51));
  GetPrefs()->SetDict(prefs::kAccessibilityFaceGazeGesturesToConfidence,
                      base::DictValue()
                          .Set("jawOpen", 0)
                          .Set("jawLeft", 150)
                          .Set("jawRight", "notANumber")
                          .Set("mouthPucker", -50)
                          .Set("eyeSquintLeft", 0));
  GetPrefs()->SetDict(
      prefs::kAccessibilityFaceGazeGesturesToKeyCombos,
      base::DictValue()
          .Set("browInnerUp", "{malformed json")
          .Set("mouthPucker", R"({"key":99999,"modifiers":{"ctrl":true}})")
          .Set("eyeSquintLeft", R"({"key":0,"modifiers":{"ctrl":true}})"));
  GetPrefs()->CommitPendingWrite();
  // `SettingsPrivateEventRouter::OnPreferenceChanged()` posts
  // `SendPrefChange()` asynchronously on the UI thread; two extension script
  // round-trips ensure the preference updates reach the extension before
  // sending mock landmarks.
  utils()->TriggerMouseControllerInterval();
  utils()->TriggerMouseControllerInterval();

  // 1. Verify that clamped or invalid bindings do not fire key events.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithNormalizedForeheadLocation(kDefaultForeheadLocation)
          // Clamps JawOpen 0% confidence to 1%, so score 0 does not fire.
          .WithGesture(MediapipeGesture::JAW_OPEN, 0)
          // Clamps JawLeft 150% confidence to 100%, so score 95 does not fire.
          .WithGesture(MediapipeGesture::JAW_LEFT, 95)
          // Resets JawRight non-numeric confidence ("notANumber") from 90% to
          // the 50% default, so score 40 does not fire.
          .WithGesture(MediapipeGesture::JAW_RIGHT, 40)
          // Ignores BrowInnerUp with invalid macro ID (999999) and malformed
          // key combination JSON.
          .WithGesture(MediapipeGesture::BROW_INNER_UP, 90)
          // Ignores MouthFunnel with non-numeric macro value (true).
          .WithGesture(MediapipeGesture::MOUTH_FUNNEL, 90)
          // Ignores MouthPucker with out-of-range key code (99999).
          .WithGesture(MediapipeGesture::MOUTH_PUCKER, 90)
          // Ignores EyeSquintLeft with unknown key code (0 = UNKNOWN).
          .WithGesture(MediapipeGesture::EYE_SQUINT_LEFT, 90));
  EXPECT_EQ(0u, event_handler().key_events().size());

  // 2. Verify that valid bindings with clamped or fallback confidence
  // thresholds fire once their thresholds are met.
  event_handler().ClearEvents();
  utils()->ProcessFaceLandmarkerResult(
      MockFaceLandmarkerResult()
          .WithNormalizedForeheadLocation(kDefaultForeheadLocation)
          // Fires JawOpen -> KEY_PRESS_SPACE at 5% (>= 1% clamped minimum).
          .WithGesture(MediapipeGesture::JAW_OPEN, 5)
          // Fires JawLeft -> KEY_PRESS_LEFT at 100% (>= 100% clamped maximum).
          .WithGesture(MediapipeGesture::JAW_LEFT, 100)
          // Fires JawRight -> KEY_PRESS_RIGHT at 60% (>= 50% default fallback).
          .WithGesture(MediapipeGesture::JAW_RIGHT, 60));
  ASSERT_EQ(3u, event_handler().key_events().size());
  EXPECT_EQ(ui::KeyboardCode::VKEY_LEFT,
            event_handler().key_events()[0].key_code());
  EXPECT_EQ(ui::EventType::kKeyPressed, event_handler().key_events()[0].type());
  EXPECT_EQ(ui::KeyboardCode::VKEY_SPACE,
            event_handler().key_events()[1].key_code());
  EXPECT_EQ(ui::EventType::kKeyPressed, event_handler().key_events()[1].type());
  EXPECT_EQ(ui::KeyboardCode::VKEY_RIGHT,
            event_handler().key_events()[2].key_code());
  EXPECT_EQ(ui::EventType::kKeyPressed, event_handler().key_events()[2].type());
}

}  // namespace ash
