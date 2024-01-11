// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/fake_system_tray_model.h"

namespace ash {

FakeSystemTrayModel::FakeSystemTrayModel() = default;

FakeSystemTrayModel::~FakeSystemTrayModel() = default;

bool FakeSystemTrayModel::IsFakeModel() const {
  return true;
}

bool FakeSystemTrayModel::IsInUserChildSession() const {
  return is_in_user_child_session_;
}

}  // namespace ash
