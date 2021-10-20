// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/fwupd/firmware_update_manager.h"

#include "base/check_op.h"

namespace ash {

namespace {

FirmwareUpdateManager* g_instance = nullptr;

}  // namespace

FirmwareUpdateManager::FirmwareUpdateManager() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

FirmwareUpdateManager::~FirmwareUpdateManager() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
FirmwareUpdateManager* FirmwareUpdateManager::Get() {
  DCHECK(g_instance);
  return g_instance;
}

}  // namespace ash
