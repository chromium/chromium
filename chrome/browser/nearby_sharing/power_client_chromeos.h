// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_CHROMEOS_H_
#define CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_CHROMEOS_H_

#include "chrome/browser/nearby_sharing/power_client.h"

#include "chromeos/dbus/power/power_manager_client.h"

class PowerClientChromeos : public PowerClient,
                            public chromeos::PowerManagerClient::Observer {
 public:
  PowerClientChromeos();
  ~PowerClientChromeos() override;

 private:
  // chromeos::PowerManagerClient::Observer:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_POWER_CLIENT_CHROMEOS_H_
