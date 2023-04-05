// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_CHROMEOS_H_

#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace chromeos::extensions {

// Chrome OS implementation of NetworkingPrivateDelegate::UIDelegate.
class NetworkingPrivateUIDelegateChromeOS
    : public ::extensions::NetworkingPrivateDelegate::UIDelegate {
 public:
  NetworkingPrivateUIDelegateChromeOS() = default;
  ~NetworkingPrivateUIDelegateChromeOS() override = default;

  NetworkingPrivateUIDelegateChromeOS(
      const NetworkingPrivateUIDelegateChromeOS&) = delete;
  NetworkingPrivateUIDelegateChromeOS& operator=(
      const NetworkingPrivateUIDelegateChromeOS&) = delete;

  // NetworkingPrivateDelegate::UIDelegate
  void ShowAccountDetails(const std::string& guid) const override;
};

}  // namespace chromeos::extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_CHROMEOS_H_
