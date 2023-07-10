// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_

#include <string>
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/extensions/api/terminal/startup_status.h"
#include "storage/browser/file_system/file_system_url.h"

namespace guest_os {

class GuestOsTerminalProvider {
 public:
  GuestOsTerminalProvider();
  virtual ~GuestOsTerminalProvider();

  GuestOsTerminalProvider(const GuestOsTerminalProvider&) = delete;
  GuestOsTerminalProvider& operator=(const GuestOsTerminalProvider&) = delete;

  // Human-friendly localised display label.
  virtual std::string Label() = 0;

  // The id of the guest this provider is for.
  virtual guest_os::GuestId GuestId() = 0;

  // Checks if recovery is required before being able to launch. If so, launches
  // the recovery flow (e.g. shows a dialog) and returns true, otherwise returns
  // false.
  virtual bool RecoveryRequired(int64_t display_id) = 0;

  // Returns false if the terminal should show this entry as being disabled by
  // the user's IT administrator, otherwise return true.
  virtual bool AllowedByPolicy() = 0;

  // Sets up `url` to be the initial working directory of a terminal session,
  // returning the path inside the guest.
  virtual std::string PrepareCwd(storage::FileSystemURL path) = 0;

  // Creates a StartupStatus which will send progress messages to the terminal
  // via the provided printer. Must pass this status to EnsureRunning.
  virtual std::unique_ptr<extensions::StartupStatus> CreateStartupStatus(
      std::unique_ptr<extensions::StartupStatusPrinter> printer) = 0;

  // Ensure the guest is running and ready for vsh connections. Calls `callback`
  // once done, setting `success` appropriately and on failure will stick a
  // human-readable error reason in `failure_reason`. Will use `startup_status`
  // to emit progress messages, this `startup_status` must be the same one
  // created by CreateStartupStatus.
  virtual void EnsureRunning(
      extensions::StartupStatus* startup_status,
      base::OnceCallback<void(bool success, std::string failure_reason)>
          callback) = 0;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_
