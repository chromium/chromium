// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_

#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider.h"

class Profile;

namespace crostini {

class CrostiniTerminalProvider : public guest_os::GuestOsTerminalProvider {
 public:
  CrostiniTerminalProvider(Profile* profile, guest_os::GuestId container_id_);
  ~CrostiniTerminalProvider() override;

  std::string Label() override;

  guest_os::GuestId GuestId() override;

  bool RecoveryRequired(int64_t display_id) override;

  std::string PrepareCwd(storage::FileSystemURL path) override;

 private:
  Profile* profile_;
  guest_os::GuestId container_id_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_
