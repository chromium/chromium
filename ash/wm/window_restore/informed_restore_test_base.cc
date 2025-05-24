// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_test_base.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

namespace {

constexpr char kTestUserEmail[] = "testuser@informedrestore";

}  // namespace

InformedRestoreTestBase::InformedRestoreTestBase() = default;

InformedRestoreTestBase::~InformedRestoreTestBase() = default;

PrefService* InformedRestoreTestBase::GetTestPrefService() {
  return GetSessionControllerClient()->GetUserPrefService(
      AccountId::FromUserEmail(kTestUserEmail));
}

void InformedRestoreTestBase::SetUp() {
  AshTestBase::SetUp();

  ClearLogin();

  // Inject our own PrefService as the restore preference is normally
  // registered in chrome/browser/ash/ and is not registered in ash unit
  // tests.
  auto test_prefs = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(test_prefs.get()->registry(), /*country=*/"",
                           /*for_test=*/true);
  // Note: normally, this pref is registered with the
  // `user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF` flag.
  test_prefs.get()->registry()->RegisterIntegerPref(
      prefs::kRestoreAppsAndPagesPrefName,
      static_cast<int>(full_restore::RestoreOption::kAskEveryTime));

  SimulateUserLogin({kTestUserEmail}, std::nullopt, std::move(test_prefs));
}

}  // namespace ash
