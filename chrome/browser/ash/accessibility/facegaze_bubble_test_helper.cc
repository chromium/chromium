// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/facegaze_bubble_test_helper.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/system/accessibility/facegaze_bubble_controller.h"
#include "ash/system/accessibility/facegaze_bubble_view.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

namespace {

FaceGazeBubbleController* GetController() {
  if (!Shell::HasInstance()) {
    return nullptr;
  }

  return Shell::Get()
      ->accessibility_controller()
      ->GetFaceGazeBubbleControllerForTest();
}

}  // namespace

FaceGazeBubbleTestHelper::FaceGazeBubbleTestHelper() = default;

FaceGazeBubbleTestHelper::~FaceGazeBubbleTestHelper() = default;

bool FaceGazeBubbleTestHelper::IsVisible() {
  return GetController()->widget_->IsVisible();
}

gfx::Point FaceGazeBubbleTestHelper::GetCloseButtonCenterPoint() {
  return GetController()
      ->facegaze_bubble_view_->close_view_->GetBoundsInScreen()
      .CenterPoint();
}

}  // namespace ash
