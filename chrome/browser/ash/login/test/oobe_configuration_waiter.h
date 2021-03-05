// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_CONFIGURATION_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_CONFIGURATION_WAITER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/ash/login/oobe_configuration.h"

namespace chromeos {

// Class that ensures that OOBE Configuration was loaded before
// proceeding with checks.
class OOBEConfigurationWaiter : public OobeConfiguration::Observer {
 public:
  OOBEConfigurationWaiter();

  ~OOBEConfigurationWaiter() override;

  // OobeConfiguration::Observer override:
  void OnOobeConfigurationChanged() override;

  // Return `true` or register wait callback until configuration is loaded.
  bool IsConfigurationLoaded(base::OnceClosure callback);

 private:
  base::OnceClosure callback_;

  DISALLOW_COPY_AND_ASSIGN(OOBEConfigurationWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_OOBE_CONFIGURATION_WAITER_H_
