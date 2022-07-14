// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_CAPABILITIES_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_CAPABILITIES_H_

#include "base/callback_forward.h"
#include "chrome/browser/ash/guest_os/guest_os_capabilities.h"

class Profile;

namespace crostini {

class CrostiniCapabilities : public guest_os::GuestOsCapabilities {
 public:
  // Builds an instance of the capabilities for the given |profile|.
  static void Build(
      Profile* profile,
      base::OnceCallback<void(std::unique_ptr<guest_os::GuestOsCapabilities>)>
          callback);

  ~CrostiniCapabilities() override;

  // exo::Capabilities overrides:
  std::string GetSecurityContext() const override;

 private:
  // Private constructor to force use of Build().
  CrostiniCapabilities() = default;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_CAPABILITIES_H_
