// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_CHROME_PROXIMITY_AUTH_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_CHROME_PROXIMITY_AUTH_CLIENT_H_

#include "base/macros.h"
#include "chromeos/components/proximity_auth/proximity_auth_client.h"

class Profile;

namespace chromeos {

// A Chrome-specific implementation of the ProximityAuthClient interface.
// There is one |ChromeProximityAuthClient| per |Profile|.
class ChromeProximityAuthClient : public proximity_auth::ProximityAuthClient {
 public:
  explicit ChromeProximityAuthClient(Profile* profile);
  ~ChromeProximityAuthClient() override;

  // proximity_auth::ProximityAuthClient:
  void UpdateScreenlockState(proximity_auth::ScreenlockState state) override;
  void FinalizeUnlock(bool success) override;
  void FinalizeSignin(const std::string& secret) override;
  void GetChallengeForUserAndDevice(
      const std::string& user_id,
      const std::string& remote_public_key,
      const std::string& nonce,
      base::Callback<void(const std::string& challenge)> callback) override;
  proximity_auth::ProximityAuthPrefManager* GetPrefManager() override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProximityAuthClient);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_EASY_UNLOCK_CHROME_PROXIMITY_AUTH_CLIENT_H_
