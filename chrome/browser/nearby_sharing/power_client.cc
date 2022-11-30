// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/power_client.h"

PowerClient::PowerClient() = default;

PowerClient::~PowerClient() = default;

void PowerClient::AddObserver(PowerClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void PowerClient::RemoveObserver(PowerClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PowerClient::IsSuspended() {
  return is_suspended_;
}

void PowerClient::SetSuspended(bool is_suspended) {
  is_suspended_ = is_suspended;
  for (auto& observer : observers_) {
    if (is_suspended)
      observer.SuspendImminent();
    else
      observer.SuspendDone();
  }
}
