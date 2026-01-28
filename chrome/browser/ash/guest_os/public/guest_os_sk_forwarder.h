// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SK_FORWARDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SK_FORWARDER_H_

#include <string>

#include "base/functional/callback_forward.h"

class Profile;

namespace guest_os {

class GuestOsSkForwarder {
 public:
  GuestOsSkForwarder();
  ~GuestOsSkForwarder();

  GuestOsSkForwarder(const GuestOsSkForwarder&) = delete;
  GuestOsSkForwarder& operator=(const GuestOsSkForwarder&) = delete;

  void DeliverMessageToSKForwardingExtension(
      Profile* profile,
      const std::string& json_message,
      base::OnceCallback<void(const std::string& response)> callback);
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_SK_FORWARDER_H_
