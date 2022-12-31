// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"

#include "base/check_op.h"

namespace ash {

namespace {
HatsBluetoothRevampTrigger* g_hats_bluetooth_revamp_trigger = nullptr;
}  // namespace

HatsBluetoothRevampTrigger::HatsBluetoothRevampTrigger() {
  DCHECK(!g_hats_bluetooth_revamp_trigger);
  g_hats_bluetooth_revamp_trigger = this;
}

// static
HatsBluetoothRevampTrigger* HatsBluetoothRevampTrigger::Get() {
  return g_hats_bluetooth_revamp_trigger;
}

HatsBluetoothRevampTrigger::~HatsBluetoothRevampTrigger() {
  DCHECK_EQ(g_hats_bluetooth_revamp_trigger, this);
  g_hats_bluetooth_revamp_trigger = nullptr;
}

}  // namespace ash