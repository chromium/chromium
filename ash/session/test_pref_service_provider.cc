// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/test_pref_service_provider.h"

#include <algorithm>

#include "ash/public/cpp/ash_prefs.h"
#include "base/check.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

// static
std::unique_ptr<TestingPrefServiceSimple>
TestPrefServiceProvider::CreateUserPrefServiceSimple() {
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(pref_service->registry(), /*country=*/"",
                           /*for_test=*/true);
  return pref_service;
}

TestPrefServiceProvider::TestPrefServiceProvider() = default;
TestPrefServiceProvider::~TestPrefServiceProvider() = default;

void TestPrefServiceProvider::CreateSigninPrefsIfNeeded() {
  if (signin_prefs_)
    return;

  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterSigninProfilePrefs(pref_service->registry(), /*country=*/"",
                             /**for_test=*/true);
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

void TestPrefServiceProvider::SetUserPrefs(
    const AccountId& account_id,
    std::unique_ptr<PrefService> pref_service) {
  CHECK_EQ(GetUserPrefs(account_id), nullptr);

  user_prefs_map_.emplace(account_id, std::move(pref_service));
}

void TestPrefServiceProvider::SetUnownedUserPrefs(
    const AccountId& account_id,
    raw_ptr<PrefService> unowned_pref_service) {
  CHECK_EQ(GetUserPrefs(account_id), nullptr);

  unowned_user_prefs_map_.emplace(account_id, std::move(unowned_pref_service));
}

PrefService* TestPrefServiceProvider::GetUserPrefs(
    const AccountId& account_id) {
  auto it = user_prefs_map_.find(account_id);
  if (it != user_prefs_map_.end()) {
    return it->second.get();
  }

  auto unowned_it = unowned_user_prefs_map_.find(account_id);
  if (unowned_it != unowned_user_prefs_map_.end()) {
    return unowned_it->second.get();
  }

  return nullptr;
}

void TestPrefServiceProvider::ClearUnownedUserPrefs(
    const AccountId& account_id) {
  auto unowned_it = unowned_user_prefs_map_.find(account_id);
  CHECK(unowned_it != unowned_user_prefs_map_.end());

  unowned_user_prefs_map_.erase(unowned_it);
}

}  // namespace ash
