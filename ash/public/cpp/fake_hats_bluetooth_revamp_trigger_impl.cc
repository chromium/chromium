// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/fake_hats_bluetooth_revamp_trigger_impl.h"
#include "ash/public/cpp/hats_bluetooth_revamp_trigger.h"

namespace ash {

FakeHatsBluetoothRevampTriggerImpl::FakeHatsBluetoothRevampTriggerImpl() =
    default;

void FakeHatsBluetoothRevampTriggerImpl::TryToShowSurvey() {
  try_to_show_survey_count_++;
}

}  // namespace ash
