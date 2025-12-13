// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/informed_restore_test_base.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/test_session_controller_client.h"
#include "components/account_id/account_id.h"

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

  auto account_id = SimulateUserLogin({kTestUserEmail});

  // Disable the onboarding dialog for testing.
  PrefService* prefs =
      ash_test_helper()->prefs_provider()->GetUserPrefs(account_id);
  prefs->SetBoolean(prefs::kShowInformedRestoreOnboarding, false);
}

}  // namespace ash
