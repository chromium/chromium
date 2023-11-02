// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/power_client_chromeos.h"

PowerClientChromeos::PowerClientChromeos() {
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->AddObserver(this);
}

PowerClientChromeos::~PowerClientChromeos() {
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerClientChromeos::SuspendImminent(
    power_manager::SuspendImminent::Reason reason) {
  SetSuspended(true);
}

void PowerClientChromeos::SuspendDone(base::TimeDelta sleep_duration) {
  SetSuspended(false);
}
