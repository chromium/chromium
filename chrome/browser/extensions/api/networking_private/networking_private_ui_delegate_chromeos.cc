// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"

#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

namespace chromeos {
namespace extensions {

NetworkingPrivateUIDelegateChromeOS::NetworkingPrivateUIDelegateChromeOS() {}

NetworkingPrivateUIDelegateChromeOS::~NetworkingPrivateUIDelegateChromeOS() {}

void NetworkingPrivateUIDelegateChromeOS::ShowAccountDetails(
    const std::string& guid) const {
  chromeos::NetworkConnect::Get()->ShowMobileSetup(guid);
}

}  // namespace extensions
}  // namespace chromeos
