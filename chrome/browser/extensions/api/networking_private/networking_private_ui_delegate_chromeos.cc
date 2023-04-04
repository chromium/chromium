// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_ui_delegate_chromeos.h"

#include "chromeos/ash/components/network/network_connect.h"

namespace chromeos::extensions {

void NetworkingPrivateUIDelegateChromeOS::ShowAccountDetails(
    const std::string& guid) const {
  ash::NetworkConnect::Get()->ShowCarrierAccountDetail(guid);
}

}  // namespace chromeos::extensions
