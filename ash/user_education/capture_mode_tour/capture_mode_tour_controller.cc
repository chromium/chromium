// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/user_education/capture_mode_tour/capture_mode_tour_controller.h"

#include "base/check_op.h"

namespace ash {
namespace {

// The singleton instance owned by the `UserEducationController`.
CaptureModeTourController* g_instance = nullptr;

}  // namespace

// CaptureModeTourController ---------------------------------------------------

CaptureModeTourController::CaptureModeTourController() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

CaptureModeTourController::~CaptureModeTourController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
CaptureModeTourController* CaptureModeTourController::Get() {
  return g_instance;
}

}  // namespace ash
