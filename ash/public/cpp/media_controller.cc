// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/media_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

MediaController* g_instance = nullptr;

}  // namespace

template <>
MediaController*&
MediaController::ScopedResetterForTest::GetGlobalInstanceHolder() {
  return g_instance;
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
