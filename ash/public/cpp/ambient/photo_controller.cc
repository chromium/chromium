// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ambient/photo_controller.h"

#include "base/logging.h"

namespace ash {

namespace {

PhotoController* g_photo_controller = nullptr;

}  // namespace

// static
PhotoController* PhotoController::Get() {
  return g_photo_controller;
}

PhotoController::PhotoController() {
  DCHECK(!g_photo_controller);
  g_photo_controller = this;
}

PhotoController::~PhotoController() {
  DCHECK_EQ(g_photo_controller, this);
  g_photo_controller = nullptr;
}

}  // namespace ash
