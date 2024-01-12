// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/model/scoped_fake_system_tray_model.h"

#include <memory>

#include "ash/public/cpp/system_tray.h"
#include "ash/shell.h"
#include "ash/system/model/fake_system_tray_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/test/ash_test_base.h"

namespace ash {

using ScopedFakeSystemTrayModelTest = AshTestBase;

TEST_F(ScopedFakeSystemTrayModelTest, Basic) {
  auto* real_instance = Shell::Get()->system_tray_model();

  // Also make sure that `SystemTray::Get()` is the same instance.
  EXPECT_EQ(real_instance, SystemTray::Get());

  // Scoped setter is created. All the public getters should point to the fake
  // instance.
  auto scoped_model = std::make_unique<ScopedFakeSystemTrayModel>();

  auto* fake_instance = scoped_model->fake_model();

  EXPECT_EQ(fake_instance, Shell::Get()->system_tray_model());
  EXPECT_EQ(fake_instance, SystemTray::Get());

  // When the scoped setter is removed. All the public getters should now point
  // to the previous real instance.
  scoped_model.reset();
  EXPECT_EQ(real_instance, Shell::Get()->system_tray_model());
  EXPECT_EQ(real_instance, SystemTray::Get());
}

}  // namespace ash
