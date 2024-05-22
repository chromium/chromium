// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/mahi/mahi_prefs_controller_ash.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "chrome/browser/chromeos/mahi/mahi_prefs_controller.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"

namespace mahi {

namespace {

PrefService* GetPrefService() {
  return ash::Shell::Get()->session_controller()->GetActivePrefService();
}

using MahiPrefsControllerAshTest = ChromeAshTestBase;

TEST_F(MahiPrefsControllerAshTest, GetMahiEnabled) {
  MahiPrefsControllerAsh controller;

  GetPrefService()->SetBoolean(ash::prefs::kMahiEnabled, true);
  EXPECT_TRUE(controller.GetMahiEnabled());

  GetPrefService()->SetBoolean(ash::prefs::kMahiEnabled, false);
  EXPECT_FALSE(controller.GetMahiEnabled());
}

TEST_F(MahiPrefsControllerAshTest, SetMahiEnabled) {
  MahiPrefsControllerAsh controller;

  controller.SetMahiEnabled(true);
  EXPECT_TRUE(GetPrefService()->GetBoolean(ash::prefs::kMahiEnabled));
  EXPECT_TRUE(controller.GetMahiEnabled());

  controller.SetMahiEnabled(false);
  EXPECT_FALSE(GetPrefService()->GetBoolean(ash::prefs::kMahiEnabled));
  EXPECT_FALSE(controller.GetMahiEnabled());
}

}  // namespace

}  // namespace mahi
