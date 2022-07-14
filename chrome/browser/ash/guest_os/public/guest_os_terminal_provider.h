// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_

#include <string>
#include "chrome/browser/ash/guest_os/guest_id.h"
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

  // TODO(b/233287586): While we're migrating, some Crostini-specific code still
  // needs a guest_os::GuestId. Eventually this should always be nullopt and
  // then removed.
  virtual guest_os::GuestId GuestId() = 0;

  // Checks if recovery is required before being able to launch. If so, launches
  // the recovery flow (e.g. shows a dialog) and returns true, otherwise returns
  // false.
  virtual bool RecoveryRequired(int64_t display_id) = 0;

  // Sets up `url` to be the initial working directory of a terminal session,
  // returning the path inside the guest.
  virtual std::string PrepareCwd(storage::FileSystemURL path) = 0;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_
