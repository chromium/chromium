// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_

#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider.h"

namespace crostini {

class CrostiniTerminalProvider : public guest_os::GuestOsTerminalProvider {
 public:
  explicit CrostiniTerminalProvider(ContainerId container_id_);
  ~CrostiniTerminalProvider() override;

  std::string Label() override;

  // TODO(b/233287586): While we're migrating some Crostini-specific code still
  // needs a ContainerId. Eventually this should always be nullopt and then
  // removed.
  absl::optional<crostini::ContainerId> CrostiniContainerId() override;

 private:
  ContainerId container_id_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_TERMINAL_PROVIDER_H_
