// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_TEST_PREF_SERVICE_PROVIDER_H_
#define ASH_SESSION_TEST_PREF_SERVICE_PROVIDER_H_

#include <map>
#include <memory>

class AccountId;
class PrefService;

namespace ash {

// Helper class to own sign-in and user pref services. This simulates production
// code that these pref services outlives session controller and its client.
// Some of the tests re-create user sessions to simulate re-login or create its
// own SessionControllerClient. This class holds on to the pref services so that
// various ash components (by way of PrefChangeRegsitrar) still holds valid
// references to them when that happens.
class TestPrefServiceProvider {
 public:
  TestPrefServiceProvider();

  TestPrefServiceProvider(const TestPrefServiceProvider&) = delete;
  TestPrefServiceProvider& operator=(const TestPrefServiceProvider&) = delete;

  ~TestPrefServiceProvider();

  void CreateSigninPrefsIfNeeded();
  void SetSigninPrefs(std::unique_ptr<PrefService> signin_prefs);
  PrefService* GetSigninPrefs();

  void CreateUserPrefs(const AccountId& account_id);
  void SetUserPrefs(const AccountId& account_id,
                    std::unique_ptr<PrefService> pref_service);
  PrefService* GetUserPrefs(const AccountId& account_id);

 private:
  std::unique_ptr<PrefService> signin_prefs_;
  std::map<AccountId, std::unique_ptr<PrefService>> user_prefs_map_;
};

}  // namespace ash

#endif  // ASH_SESSION_TEST_PREF_SERVICE_PROVIDER_H_
