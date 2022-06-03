// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/test_pref_service_provider.h"

#include <algorithm>

#include "ash/public/cpp/ash_prefs.h"
#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

TestPrefServiceProvider::TestPrefServiceProvider() = default;
TestPrefServiceProvider::~TestPrefServiceProvider() = default;

void TestPrefServiceProvider::CreateSigninPrefsIfNeeded() {
  if (signin_prefs_)
    return;

  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterSigninProfilePrefs(pref_service->registry(), true /* for_test */);
  signin_prefs_ = std::move(pref_service);
}

void TestPrefServiceProvider::SetSigninPrefs(
    std::unique_ptr<PrefService> signin_prefs) {
  DCHECK(!signin_prefs_);
  signin_prefs_ = std::move(signin_prefs);
}

PrefService* TestPrefServiceProvider::GetSigninPrefs() {
  return signin_prefs_.get();
}

void TestPrefServiceProvider::CreateUserPrefs(const AccountId& account_id) {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(pref_service->registry(), true /* for_test */);
  SetUserPrefs(account_id, std::move(pref_service));
}

void TestPrefServiceProvider::SetUserPrefs(
    const AccountId& account_id,
    std::unique_ptr<PrefService> pref_service) {
  DCHECK(user_prefs_map_.find(account_id) == user_prefs_map_.end());
  user_prefs_map_[account_id] = std::move(pref_service);
}

PrefService* TestPrefServiceProvider::GetUserPrefs(
    const AccountId& account_id) {
  auto it = user_prefs_map_.find(account_id);
  if (it == user_prefs_map_.end())
    return nullptr;

  return it->second.get();
}

}  // namespace ash
