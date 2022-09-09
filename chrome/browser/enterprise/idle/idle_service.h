// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class Profile;

namespace enterprise_idle {

// Manages the state of a profile for the IdleProfileCloseTimeout enterprise
// policy. Keeps track of the policy's value, and listens for idle events.
// Closes the profile's window when it becomes idle, and shows the profile
// picker.
class IdleService : public KeyedService {
 public:
  explicit IdleService(Profile* profile);

  IdleService(const IdleService&) = delete;
  IdleService& operator=(const IdleService&) = delete;

  ~IdleService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  // Called when the IdleProfileCloseTimeout policy changes, via the
  // "idle_profile_close_timeout" pref it's mapped to.
  void OnIdleProfileCloseTimeoutPrefChanged();

  PrefChangeRegistrar pref_change_registrar_;

  Profile* profile_;
};

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_SERVICE_H_
