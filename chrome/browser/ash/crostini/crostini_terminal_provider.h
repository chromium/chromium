// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_

#include "base/memory/raw_ptr.h"
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

  bool AllowedByPolicy() override;

  std::string PrepareCwd(storage::FileSystemURL path) override;

  std::unique_ptr<extensions::StartupStatus> CreateStartupStatus(
      std::unique_ptr<extensions::StartupStatusPrinter> printer) override;

  void EnsureRunning(
      extensions::StartupStatus* startup_status,
      base::OnceCallback<void(bool success, std::string failure_reason)>
          callback) override;

 private:
  raw_ptr<Profile> profile_;
  guest_os::GuestId container_id_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_
