// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ANNOTATOR_ANNOTATOR_TEST_UTIL_H_
#define ASH_ANNOTATOR_ANNOTATOR_TEST_UTIL_H_

#include "ash/capture_mode/capture_mode_types.h"
#include "ash/webui/annotator/test/mock_annotator_client.h"
#include "base/test/scoped_feature_list.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

void ExpectChildOfMenuContainer(aura::Window* overlay_window,
                                aura::Window* source_window);
void ExpectSameWindowBounds(aura::Window* overlay_window,
                            aura::Window* source_window);
void VerifyWindowStackingOnRoot(aura::Window* overlay_window,
                                aura::Window* source_window);
void VerifyWindowStackingOnTestWindow(aura::Window* overlay_window,
                                      aura::Window* source_window);
void VerifyWindowStackingOnRegion(aura::Window* overlay_window,
                                  aura::Window* source_window,
                                  const gfx::Rect region_bounds);
void VerifyOverlayEnabledState(aura::Window* overlay_window,
                               bool overlay_enabled_state);
void VerifyOverlayWindowForCaptureMode(aura::Window* overlay_window,
                                       aura::Window* window_being_recorded,
                                       CaptureModeSource source,
                                       const gfx::Rect region_bounds);

// Defines a helper class to allow setting up and testing the annotator feature
// in multiple test fixtures.
class AnnotatorIntegrationHelper {
 public:
  AnnotatorIntegrationHelper();
  AnnotatorIntegrationHelper(const AnnotatorIntegrationHelper&) = delete;
  AnnotatorIntegrationHelper& operator=(const AnnotatorIntegrationHelper&) =
      delete;
  ~AnnotatorIntegrationHelper() = default;

  // Sets up the annotator client. Must be called after `AshTestBase::SetUp()`
  // has been called.
  void SetUp();

 private:
  MockAnnotatorClient annotator_client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};
}  // namespace ash

#endif  // ASH_ANNOTATOR_ANNOTATOR_TEST_UTIL_H_
