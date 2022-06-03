// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/toast_manager.h"

#include "base/check_op.h"

namespace ash {

namespace {

ToastManager* g_instance = nullptr;

}  // namespace

// static
ToastManager* ToastManager::Get() {
  return g_instance;
}

ToastManager::ToastManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

ToastManager::~ToastManager() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
