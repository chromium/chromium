// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_OSAUTH_CHROME_AUTH_PARTS_H_
#define CHROME_BROWSER_ASH_LOGIN_OSAUTH_CHROME_AUTH_PARTS_H_

#include <memory>

#include "base/scoped_observation.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"

class PrefService;

namespace ash {

// Creates and owns `ash::AuthParts` instance and provides it with
// browser-specific implementations.
class ChromeAuthParts : public ash::SessionTerminationManager::Observer {
 public:
  // `local_state` must be non-null and must outlive `this`.
  explicit ChromeAuthParts(PrefService* local_state);
  ~ChromeAuthParts() override;

 private:
  void OnAppTerminating() override;

  base::ScopedObservation<ash::SessionTerminationManager,
                          ash::SessionTerminationManager::Observer>
      observation_{this};
  std::unique_ptr<AuthParts> auth_parts_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_OSAUTH_CHROME_AUTH_PARTS_H_
