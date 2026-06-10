// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_SESSION_ARC_ACTIVATION_NECESSITY_CHECKER_H_
#define CHROME_BROWSER_ASH_ARC_SESSION_ARC_ACTIVATION_NECESSITY_CHECKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

class Profile;

namespace arc {

// ArcActivationNecessityChecker checks if it's necessary to activate ARC
// without the user's action.
class ArcActivationNecessityChecker {
 public:
  explicit ArcActivationNecessityChecker(Profile* profile);
  ArcActivationNecessityChecker(const ArcActivationNecessityChecker&) = delete;
  ArcActivationNecessityChecker& operator=(
      const ArcActivationNecessityChecker&) = delete;
  ~ArcActivationNecessityChecker();

  // Checks if it's necessary to activate ARC without the user's action, and
  // runs the callback with true if it's necessary to activate ARC.
  using CheckCallback = base::OnceCallback<void(bool result)>;
  void Check(CheckCallback callback);

 private:
  void OnChecked(CheckCallback callback, bool result);
  void OnQueryAdbSideload(
      CheckCallback callback,
      ash::SessionManagerClient::AdbSideloadResponseCode response_code,
      bool is_allowed);

  const raw_ptr<Profile> profile_;
  base::WeakPtrFactory<ArcActivationNecessityChecker> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_SESSION_ARC_ACTIVATION_NECESSITY_CHECKER_H_
