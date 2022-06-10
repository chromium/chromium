// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_terminal_provider.h"

#include "base/strings/strcat.h"
#include "chrome/browser/ash/crostini/crostini_util.h"

namespace crostini {

CrostiniTerminalProvider::CrostiniTerminalProvider(ContainerId container_id)
    : container_id_(container_id) {}
CrostiniTerminalProvider::~CrostiniTerminalProvider() = default;

std::string CrostiniTerminalProvider::Label() {
  if (container_id_.vm_name == kCrostiniDefaultVmName) {
    return container_id_.container_name;
  }
  return base::StrCat(
      {container_id_.vm_name, ":", container_id_.container_name});
}

absl::optional<crostini::ContainerId>
CrostiniTerminalProvider::CrostiniContainerId() {
  return container_id_;
}

}  // namespace crostini
