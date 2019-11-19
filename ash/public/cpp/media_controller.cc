// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/media_controller.h"

#include "base/logging.h"

namespace ash {

namespace {

MediaController* g_instance = nullptr;

}  // namespace

MediaController::ScopedResetterForTest::ScopedResetterForTest()
    : instance_(g_instance) {
  g_instance = nullptr;
}

MediaController::ScopedResetterForTest::~ScopedResetterForTest() {
  g_instance = instance_;
}

// static
MediaController* MediaController::Get() {
  return g_instance;
}

MediaController::MediaController() {
  DCHECK(!g_instance);
  g_instance = this;
}

MediaController::~MediaController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
