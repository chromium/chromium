// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_CHROME_PROXIMITY_AUTH_CLIENT_H_
#define CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_CHROME_PROXIMITY_AUTH_CLIENT_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/proximity_auth/proximity_auth_client.h"

class Profile;

namespace ash {

// A Chrome-specific implementation of the ProximityAuthClient interface.
// There is one `ChromeProximityAuthClient` per `Profile`.
class ChromeProximityAuthClient : public proximity_auth::ProximityAuthClient {
 public:
  explicit ChromeProximityAuthClient(Profile* profile);

  ChromeProximityAuthClient(const ChromeProximityAuthClient&) = delete;
  ChromeProximityAuthClient& operator=(const ChromeProximityAuthClient&) =
      delete;

  ~ChromeProximityAuthClient() override;

  // proximity_auth::ProximityAuthClient:
  void UpdateSmartLockState(SmartLockState state) override;
  void FinalizeUnlock(bool success) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SMART_LOCK_CHROME_PROXIMITY_AUTH_CLIENT_H_
