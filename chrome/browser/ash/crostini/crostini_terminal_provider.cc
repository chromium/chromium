// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal_provider.h"

#include "chrome/browser/ash/crostini/crostini_util.h"

namespace crostini {

CrostiniTerminalProvider::CrostiniTerminalProvider(
    guest_os::GuestId container_id)
    : container_id_(container_id) {}
CrostiniTerminalProvider::~CrostiniTerminalProvider() = default;

std::string CrostiniTerminalProvider::Label() {
  return crostini::FormatForUi(container_id_);
}

absl::optional<guest_os::GuestId>
CrostiniTerminalProvider::CrostiniContainerId() {
  return container_id_;
}

}  // namespace crostini
