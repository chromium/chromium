// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_

#include <string>
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  virtual absl::optional<guest_os::GuestId> CrostiniContainerId() = 0;

 private:
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_PUBLIC_GUEST_OS_TERMINAL_PROVIDER_H_
