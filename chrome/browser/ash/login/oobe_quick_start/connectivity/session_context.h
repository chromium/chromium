// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_SESSION_CONTEXT_H_
#define CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_SESSION_CONTEXT_H_

#include <array>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"

namespace ash::quick_start {

// Generates and stores unique Quick Start session information. When attempting
// to resume a Quick Start session after a reboot, the session information is
// retrieved from local state.
class SessionContext {
 public:
  using SharedSecret = std::array<uint8_t, 32>;
  using SessionId = uint64_t;

  SessionContext();

  // Alternative constructor used for testing.
  SessionContext(SessionId session_id,
                 AdvertisingId advertising_id,
                 SharedSecret shared_secret,
                 SharedSecret secondary_shared_secret,
                 bool is_resume_after_update = false);

  SessionContext(const SessionContext& other);
  SessionContext& operator=(const SessionContext& other);
  ~SessionContext();

  // Updates session info with new random values or persisted session. Used when
  // advertising begins, so that we have new session info when a user exits
  // Quick Start and attempts to re-enter.
  void FillOrResetSession();

  // resets |is_resume_after_update_| to default false value. Called when an
  // attempt to resume fails after a timeout.
  void CancelResume();

  SessionId session_id() const { return session_id_; }

  AdvertisingId advertising_id() const { return advertising_id_; }

  SharedSecret shared_secret() const { return shared_secret_; }

  SharedSecret secondary_shared_secret() const {
    return secondary_shared_secret_;
  }

  bool is_resume_after_update() const { return is_resume_after_update_; }

  // Returns Dict that can be persisted to a local state Dict pref if the target
  // device is going to update. This Dict contains the AdvertisingId and
  // secondary SharedSecret represented as base64-encoded strings. These values
  // are needed to resume the Quick Start connection after the target device
  // reboots.
  base::Value::Dict GetPrepareForUpdateInfo();

  bool did_transfer_wifi() const { return did_transfer_wifi_; }

  void SetDidTransferWifi(bool did_transfer_wifi);

 private:
  void PopulateRandomSessionContext();
  // When Quick Start is automatically resumed after the target device updates,
  // this method retrieves the previously-persisted |advertising_id| and
  // |shared_secret|.
  void FetchPersistedSessionContext();
  void DecodeSharedSecret(const std::string& encoded_shared_secret);

  SessionId session_id_;
  AdvertisingId advertising_id_;
  SharedSecret shared_secret_;
  SharedSecret secondary_shared_secret_;
  bool is_resume_after_update_ = false;
  bool did_transfer_wifi_ = false;
};

}  // namespace ash::quick_start

#endif  // CHROME_BROWSER_ASH_LOGIN_OOBE_QUICK_START_CONNECTIVITY_SESSION_CONTEXT_H_
