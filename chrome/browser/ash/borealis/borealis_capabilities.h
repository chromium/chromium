// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CAPABILITIES_H_
#define CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CAPABILITIES_H_

#include "base/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_os_capabilities.h"

class Profile;

namespace borealis {

// Controls what specialized behaviours the exo server will have when dealing
// with borealis clients.
class BorealisCapabilities : public guest_os::GuestOsCapabilities {
 public:
  // Builds an instance of the capabilities for the given |profile|.
  static void Build(
      Profile* profile,
      base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsCapabilities>)>
          callback);

  ~BorealisCapabilities() override;

  // exo::Capabilities overrides:
  std::string GetSecurityContext() const override;
};

}  // namespace borealis

#endif  // CHROME_BROWSER_ASH_BOREALIS_BOREALIS_CAPABILITIES_H_
