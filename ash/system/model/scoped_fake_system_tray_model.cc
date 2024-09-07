// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/scoped_fake_system_tray_model.h"

#include "ash/public/cpp/system_tray.h"
#include "ash/shell.h"
#include "ash/system/model/fake_system_tray_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/check.h"
#include "base/notreached.h"

namespace ash {

// static
ScopedFakeSystemTrayModel* ScopedFakeSystemTrayModel::instance_ = nullptr;

ScopedFakeSystemTrayModel::ScopedFakeSystemTrayModel() {
  // Only allow one scoped instance at a time.
  CHECK(!instance_);
  instance_ = this;

  real_system_tray_model_instance_ =
      std::move(Shell::Get()->system_tray_model_);

  // Create a fake model and replace it with the real one.
  auto fake_system_tray_model = std::make_unique<FakeSystemTrayModel>();
  fake_model_ = fake_system_tray_model.get();

  Shell::Get()->system_tray_model_ = std::move(fake_system_tray_model);
}

ScopedFakeSystemTrayModel::~ScopedFakeSystemTrayModel() {
  if (instance_ != this) {
    NOTREACHED();
  }

  instance_ = nullptr;
  fake_model_ = nullptr;
  Shell::Get()->system_tray_model_ =
      std::move(real_system_tray_model_instance_);
}

}  // namespace ash
